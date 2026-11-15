#include "MemoryProvider.h"

#include "core/Validate.h"
#include "logging/Logger.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace std::chrono_literals;

namespace {

    std::string narrow(const std::wstring& w) {
        std::string s;
        for (wchar_t c : w) s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        return s;
    }

    std::string join(const std::vector<std::string>& v) {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += v[i]; }
        return s;
    }

    std::string hex(uint64_t v) {
        char b[24];
        std::snprintf(b, sizeof(b), "0x%llX", static_cast<unsigned long long>(v));
        return b;
    }

    // Serves field reads from one block read of the entity struct when the used
    // fields span <= 4 KiB, otherwise falls back to one read per field.
    class EntityFetcher {
    public:
        EntityFetcher(const Process& p, uint64_t addr, uint64_t lo, uint64_t hi)
            : p_(p), addr_(addr), lo_(lo) {
            if (hi > lo && hi - lo <= 0x1000) {
                block_.resize(static_cast<size_t>(hi - lo));
                have_ = p_.readRaw(static_cast<uintptr_t>(addr_ + lo_), block_.data(), block_.size());
            }
        }
        bool get(uint64_t off, void* dst, size_t n) const {
            if (have_ && off >= lo_ && off + n <= lo_ + block_.size()) {
                std::memcpy(dst, block_.data() + (off - lo_), n);
                return true;
            }
            return p_.readRaw(static_cast<uintptr_t>(addr_ + off), dst, n);
        }
    private:
        const Process& p_;
        uint64_t addr_, lo_;
        std::vector<uint8_t> block_;
        bool have_ = false;
    };

} // namespace

void MemoryProvider::configure(const std::wstring& processName, const SdkOffsets& offsets) {
    std::lock_guard<std::mutex> lk(cfgMtx_);
    cfg_.processName = processName;
    cfg_.offsets = offsets;
    cfgDirty_ = true;
}

MemoryProvider::Config MemoryProvider::snapshotConfig() {
    std::lock_guard<std::mutex> lk(cfgMtx_);
    if (cfgDirty_) {
        active_ = cfg_;
        cfgDirty_ = false;
        proc_.detach();
        knownRunning_ = false;
        warnedNoModuleSize_ = false;
        lastFind_ = {}; lastAttach_ = {};
    }
    return active_;
}

void MemoryProvider::release() {
    if (proc_.isAttached()) logInfo("detached from process");
    proc_.detach();
    knownRunning_ = false;
    lastFind_ = {};
}

ProbeResult MemoryProvider::probe() {
    const Config c = snapshotConfig();
    const DWORD pid = Process::findPid(c.processName.c_str());
    return { pid != 0, static_cast<uint32_t>(pid) };
}

UpdateResult MemoryProvider::update(WorldSnap& out) {
    using Code = UpdateResult::Code;
    const Config c = snapshotConfig();
    const auto now = std::chrono::steady_clock::now();

    // Drop a dead attachment.
    if (proc_.isAttached() && !proc_.stillAlive()) {
        logInfo("target process exited");
        proc_.detach();
        knownRunning_ = false;
        lastFind_ = {};
    }

    // Is the process there? Throttled so a missing game costs ~2 lookups/second.
    if (!proc_.isAttached()) {
        if (now - lastFind_ >= 500ms || lastFind_ == std::chrono::steady_clock::time_point{}) {
            knownRunning_ = Process::findPid(c.processName.c_str()) != 0;
            lastFind_ = now;
        }
        if (!knownRunning_)
            return { Code::NotRunning, narrow(c.processName) + " is not running" };
    }

    const auto missing = c.offsets.missingRequired();
    if (!missing.empty()) {
        proc_.detach();   // do not hold a handle we have nothing to do with
        return { Code::SdkNotConfigured, "offsets.ini is missing required values: " + join(missing) };
    }

    if (!proc_.isAttached()) {
        if (now - lastAttach_ < 1s)
            return { Code::AttachFailed, lastAttachError_.empty() ? "retrying attach" : lastAttachError_ };
        lastAttach_ = now;
        std::string err;
        if (!proc_.attach(c.processName.c_str(), &err)) {
            if (err != lastAttachError_) logWarn("attach failed: {}", err);
            lastAttachError_ = err;
            return { Code::AttachFailed, err };
        }
        lastAttachError_.clear();
        logInfo("attached: pid {}, module base {}, size {}", proc_.pid(), hex(proc_.base()), hex(proc_.size()));

        if (c.offsets.expectedModuleSize) {
            if (*c.offsets.expectedModuleSize != proc_.size()) {
                const std::string d = "module size " + hex(proc_.size()) + " does not match offsets.ini build.module_size " +
                                      hex(*c.offsets.expectedModuleSize) + " - offsets are for a different build";
                logError("{}", d);
                proc_.detach();
                lastAttachError_ = d;
                return { Code::ModuleMismatch, d };
            }
            logInfo("build identity verified (module size matches offsets.ini)");
        } else if (!warnedNoModuleSize_) {
            warnedNoModuleSize_ = true;
            logWarn("offsets.ini has no build.module_size - cannot verify these offsets belong to this build");
        }
    } else if (c.offsets.expectedModuleSize && *c.offsets.expectedModuleSize != proc_.size()) {
        return { Code::ModuleMismatch, "module size mismatch" };
    }

    return readWorld(c.offsets, out);
}

UpdateResult MemoryProvider::readWorld(const SdkOffsets& o, WorldSnap& out) {
    using Code = UpdateResult::Code;
    auto fail = [](std::string d) { return UpdateResult{ Code::ReadFailed, std::move(d) }; };

    WorldSnap s;
    s.source = DataSource::Live;
    const uint64_t base = proc_.base();

    // View matrix
    if (!proc_.read<Matrix4>(static_cast<uintptr_t>(base + *o.viewMatrix), s.viewMatrix))
        return fail("cannot read view matrix (rva.view_matrix " + hex(*o.viewMatrix) + ")");
    if (!validate::viewMatrixUsable(s.viewMatrix))
        return fail("view matrix is all zero / non-finite - not in a match, or wrong rva.view_matrix");

    // Game state (optional)
    if (o.gameState) {
        int32_t st = 0;
        if (proc_.read<int32_t>(static_cast<uintptr_t>(base + *o.gameState), st)) s.inMatch = (st != 0);
    }

    // Local player
    uint64_t localPtr = 0;
    if (!proc_.read<uint64_t>(static_cast<uintptr_t>(base + *o.localPlayer), localPtr) ||
        !validate::plausiblePointer(localPtr))
        return fail("local player pointer is null/implausible (not in a match, or wrong rva.local_player)");
    s.localAddress = localPtr;
    if (!proc_.read<Vec3>(static_cast<uintptr_t>(localPtr + *o.position), s.localPos) ||
        !validate::plausibleWorldPos(s.localPos))
        return fail("local player position unreadable/implausible (wrong entity.position?)");
    proc_.read<int32_t>(static_cast<uintptr_t>(localPtr + *o.team), s.localTeam);

    // Entity list
    uint64_t listPtr = 0;
    if (!proc_.read<uint64_t>(static_cast<uintptr_t>(base + *o.entityList), listPtr) ||
        !validate::plausiblePointer(listPtr))
        return fail("entity list pointer is null/implausible (wrong rva.entity_list?)");

    uint32_t count = o.maxEntities;
    if (o.entityCount) {
        int32_t n = 0;
        if (proc_.read<int32_t>(static_cast<uintptr_t>(base + *o.entityCount), n) && n >= 0 &&
            static_cast<uint32_t>(n) <= 4096)
            count = static_cast<uint32_t>(n);
    }
    count = std::min<uint32_t>(count, 4096);

    // Block-read window over all used per-entity fields.
    uint64_t lo = ~0ull, hi = 0;
    auto span = [&](const std::optional<uint64_t>& off, uint64_t sz) {
        if (!off) return;
        lo = std::min(lo, *off);
        hi = std::max(hi, *off + sz);
    };
    span(o.position, sizeof(Vec3));  span(o.health, 4);  span(o.maxHealth, 4);  span(o.team, 4);
    span(o.isAlive, o.isAliveSize);  span(o.headPosition, sizeof(Vec3));        span(o.visible, 4);
    span(o.isLocalFlag, 1);
    span(o.name, o.nameIsPointer ? 8 : o.nameMaxLen);

    s.entities.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t ent = 0;
        if (!proc_.read<uint64_t>(static_cast<uintptr_t>(listPtr + static_cast<uint64_t>(i) * o.entityStride), ent) ||
            !validate::plausiblePointer(ent))
            continue;

        EntityFetcher f(proc_, ent, lo, hi);
        EntitySnap e;
        e.address = ent;

        if (!f.get(*o.position, &e.pos, sizeof(Vec3)) || !validate::plausibleWorldPos(e.pos)) continue;

        if (o.healthIsInt32) {
            int32_t h = 0, m = 100;
            if (!f.get(*o.health, &h, 4)) continue;
            if (o.maxHealth) f.get(*o.maxHealth, &m, 4);
            e.hp = static_cast<float>(h); e.maxHp = static_cast<float>(m);
        } else {
            if (!f.get(*o.health, &e.hp, 4)) continue;
            e.maxHp = 100.f;
            if (o.maxHealth) f.get(*o.maxHealth, &e.maxHp, 4);
        }
        if (!validate::plausibleHealth(e.hp, e.maxHp)) continue;

        int32_t team = 0;
        if (!f.get(*o.team, &team, 4)) continue;
        e.team = team;

        bool aliveFlag = true;
        if (o.isAlive) {
            uint32_t a = 0;
            if (f.get(*o.isAlive, &a, o.isAliveSize)) aliveFlag = (o.isAliveSize == 1 ? (a & 0xFF) : a) != 0;
        }
        e.alive = aliveFlag && e.hp > 0.f;

        if (o.visible) { uint32_t v = 1; f.get(*o.visible, &v, 4); e.visible = v != 0; }

        if (o.headPosition) {
            Vec3 h;
            if (f.get(*o.headPosition, &h, sizeof(Vec3)) && validate::plausibleWorldPos(h)) e.head = h;
            else e.head = { e.pos.x, e.pos.y, e.pos.z + 1.8f };
        } else {
            e.head = { e.pos.x, e.pos.y, e.pos.z + 1.8f };
        }

        if (o.name) {
            std::vector<char> buf(o.nameMaxLen + 1, 0);
            bool ok = false;
            if (o.nameIsPointer) {
                uint64_t sp = 0;
                if (f.get(*o.name, &sp, 8) && validate::plausiblePointer(sp))
                    ok = proc_.readRaw(static_cast<uintptr_t>(sp), buf.data(), o.nameMaxLen);
            } else {
                ok = f.get(*o.name, buf.data(), o.nameMaxLen);
            }
            if (ok) {
                buf[o.nameMaxLen] = '\0';
                std::string n(buf.data());
                if (validate::printableAscii(n)) e.name = std::move(n);
            }
        }

        bool isLocal = (ent == localPtr);
        if (o.isLocalFlag) { uint8_t b = 0; if (f.get(*o.isLocalFlag, &b, 1)) isLocal = isLocal || b != 0; }
        e.isLocal = isLocal;

        s.entities.push_back(std::move(e));
    }

    s.valid = true;
    out = std::move(s);
    return {};
}
