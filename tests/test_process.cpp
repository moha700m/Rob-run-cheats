// Windows-only tests of the real memory path. The "game" here is a fixture that
// lives inside the test process's own image (NOT RobRun): it lets the actual
// OpenProcess / ReadProcessMemory / MemoryProvider code run end to end.
#include "TestFramework.h"

#include "game/MemoryProvider.h"
#include "platform/MemoryScan.h"
#include "platform/Process.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>

namespace {

    std::wstring selfExeName() {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring p = buf;
        return p.substr(p.find_last_of(L"\\/") + 1);
    }

    uint64_t moduleBase() { return reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr)); }

    template <class T> uint64_t rva(const T* p) { return reinterpret_cast<uint64_t>(p) - moduleBase(); }

    struct FixtureEntity {
        float pos[3];
        float hp;
        float maxHp;
        int   team;
        char  name[32];
    };

    FixtureEntity  g_ents[5];
    FixtureEntity* g_entPtrs[8];
    FixtureEntity** g_listPtr = g_entPtrs;       // location holding "pointer to entity array"
    FixtureEntity* g_localPtr = &g_ents[0];      // location holding "pointer to local player"
    float          g_view[16];

    void fillFixture() {
        std::memset(g_ents, 0, sizeof(g_ents));
        std::memset(g_entPtrs, 0, sizeof(g_entPtrs));
        auto set = [](FixtureEntity& e, float x, float y, float z, float hp, int team, const char* n) {
            e.pos[0] = x; e.pos[1] = y; e.pos[2] = z; e.hp = hp; e.maxHp = 100.f; e.team = team;
            std::strncpy(e.name, n, sizeof(e.name) - 1);
        };
        set(g_ents[0], 0, 0, 0, 100, 0, "local");
        set(g_ents[1], 10, 20, 0, 80, 1, "alpha");
        set(g_ents[2], -5, 30, 0, 25, 1, "beta");
        set(g_ents[3], std::numeric_limits<float>::quiet_NaN(), 0, 0, 50, 1, "nanpos");    // must be skipped
        set(g_ents[4], 1, 1, 0, std::numeric_limits<float>::infinity(), 1, "infhp");       // must be skipped
        g_entPtrs[0] = &g_ents[0];
        g_entPtrs[1] = &g_ents[1];
        g_entPtrs[2] = &g_ents[2];
        g_entPtrs[3] = &g_ents[3];
        g_entPtrs[4] = &g_ents[4];
        g_entPtrs[5] = nullptr;      // empty slot
        g_listPtr = g_entPtrs;
        g_localPtr = &g_ents[0];
        std::memset(g_view, 0, sizeof(g_view));
        g_view[0] = 1.f; g_view[5] = 1.f; g_view[10] = 1.f; g_view[15] = 1.f;
    }

    SdkOffsets fixtureOffsets() {
        SdkOffsets o;
        o.buildLabel = "test fixture inside RobRunTests.exe - NOT a RobRun build";
        o.viewMatrix  = rva(g_view);
        o.localPlayer = rva(&g_localPtr);
        o.entityList  = rva(&g_listPtr);
        o.position = offsetof(FixtureEntity, pos);
        o.health   = offsetof(FixtureEntity, hp);
        o.maxHealth = offsetof(FixtureEntity, maxHp);
        o.team     = offsetof(FixtureEntity, team);
        o.name     = offsetof(FixtureEntity, name);
        o.nameMaxLen = 32;
        o.maxEntities = 8;
        return o;
    }

    // 16 bytes unlikely to occur anywhere else in the image.
    alignas(16) const volatile uint8_t kMagic[16] = { 0x5A, 0xC3, 0x91, 0x7E, 0x14, 0xD8, 0x2B, 0xF0,
                                                      0x66, 0x09, 0xAE, 0x3D, 0x81, 0x4C, 0xB7, 0xE2 };
}

TEST(process, absent_process_is_not_found_and_attach_fails_cleanly) {
    CHECK_EQ(Process::findPid(L"__robrun_definitely_not_running__.exe"), DWORD(0));
    Process p;
    std::string err;
    CHECK(!p.attach(L"__robrun_definitely_not_running__.exe", &err));
    CHECK(!p.isAttached());
    CHECK(!err.empty());
    uint32_t v = 0;
    CHECK(!p.read<uint32_t>(0x1000, v));   // reads on a detached Process fail, never crash
}

TEST(process, attach_to_self_resolves_module_and_reads_memory) {
    Process p;
    std::string err;
    CHECK(p.attach(selfExeName().c_str(), &err));
    CHECK(p.isAttached() && p.stillAlive());
    CHECK_EQ(uint64_t(p.base()), moduleBase());
    CHECK(p.size() > 0x1000);

    uint8_t got[16]{};
    CHECK(p.readRaw(reinterpret_cast<uintptr_t>(&kMagic[0]), got, sizeof(got)));
    for (int i = 0; i < 16; ++i) CHECK_EQ(int(got[i]), int(kMagic[i]));

    uint8_t big[4]{};
    CHECK(!p.readRaw(0x10, big, 4));                          // unmapped
    p.detach();
    CHECK(!p.isAttached());
}

TEST(process, pattern_scan_finds_signature_with_wildcards_in_module) {
    Process p;
    CHECK(p.attach(selfExeName().c_str()));
    // Build the signature text from the magic bytes, wildcarding two of them.
    char sigText[128];
    std::snprintf(sigText, sizeof(sigText), "%02X %02X ? %02X %02X %02X ? %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  kMagic[0], kMagic[1], kMagic[3], kMagic[4], kMagic[5], kMagic[7], kMagic[8], kMagic[9],
                  kMagic[10], kMagic[11], kMagic[12], kMagic[13], kMagic[14], kMagic[15]);
    const uintptr_t hit = mem::findPattern(p, sigText);
    CHECK_EQ(uint64_t(hit), reinterpret_cast<uint64_t>(&kMagic[0]));
    CHECK_EQ(uint64_t(mem::findPattern(p, "FF EE DD CC BB AA 99 88 77 66 55 44 33 22 11 00 FE DC BA 98 76 54 32 10 01 23")), uint64_t(0)) ;   // sanity: absent signature -> 0
}

TEST(process, provider_reports_not_running_for_absent_process) {
    MemoryProvider mp;
    mp.configure(L"__robrun_definitely_not_running__.exe", fixtureOffsets());
    CHECK(!mp.probe().found);
    WorldSnap s;
    auto r = mp.update(s);
    CHECK(r.code == UpdateResult::Code::NotRunning);
    CHECK(!s.valid);
}

TEST(process, provider_refuses_to_read_without_complete_offsets) {
    MemoryProvider mp;
    mp.configure(selfExeName(), SdkOffsets{});
    CHECK(mp.probe().found);
    WorldSnap s;
    auto r = mp.update(s);
    CHECK(r.code == UpdateResult::Code::SdkNotConfigured);
    CHECK(r.detail.find("rva.view_matrix") != std::string::npos);
    CHECK(!s.valid);
}

TEST(process, provider_detects_wrong_build_by_module_size) {
    fillFixture();
    SdkOffsets o = fixtureOffsets();
    o.expectedModuleSize = 0x1234;   // deliberately wrong
    MemoryProvider mp;
    mp.configure(selfExeName(), o);
    WorldSnap s;
    auto r = mp.update(s);
    CHECK(r.code == UpdateResult::Code::ModuleMismatch);
    CHECK(!s.valid);
}

TEST(process, provider_reads_fixture_world_end_to_end) {
    fillFixture();
    MemoryProvider mp;
    mp.configure(selfExeName(), fixtureOffsets());
    WorldSnap s;
    auto r = mp.update(s);
    CHECK(r.ok());
    CHECK(s.valid);
    CHECK(s.source == DataSource::Live);
    CHECK_EQ(s.localTeam, 0);
    CHECK_NEAR(s.viewMatrix.m[0][0], 1.0, 1e-6);
    // local + alpha + beta; NaN-position and infinite-health entities and the null slot are rejected
    CHECK_EQ(s.entities.size(), size_t(3));
    int locals = 0;
    for (const auto& e : s.entities) if (e.isLocal) ++locals;
    CHECK_EQ(locals, 1);
    const auto& alpha = s.entities[1];
    CHECK_EQ(alpha.name, std::string("alpha"));
    CHECK_NEAR(alpha.pos.x, 10.0, 1e-6);
    CHECK_NEAR(alpha.pos.y, 20.0, 1e-6);
    CHECK_NEAR(alpha.hp, 80.0, 1e-6);
    CHECK_EQ(alpha.team, 1);
    CHECK(alpha.alive);
    mp.release();
}

TEST(process, provider_rejects_unusable_view_matrix) {
    fillFixture();
    std::memset(g_view, 0, sizeof(g_view));
    MemoryProvider mp;
    mp.configure(selfExeName(), fixtureOffsets());
    WorldSnap s;
    auto r = mp.update(s);
    CHECK(r.code == UpdateResult::Code::ReadFailed);
    CHECK(!s.valid);
    fillFixture();
}

TEST(process, provider_rejects_null_local_player_pointer) {
    fillFixture();
    g_localPtr = nullptr;
    MemoryProvider mp;
    mp.configure(selfExeName(), fixtureOffsets());
    WorldSnap s;
    CHECK(mp.update(s).code == UpdateResult::Code::ReadFailed);
    fillFixture();
}
