#include "TestFramework.h"
#include "config/Settings.h"
#include "core/Keys.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static fs::path tmpDir(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("robrun_test_") + tag);
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

TEST(settings, defaults_are_sane_and_survive_sanitize) {
    Settings s;
    CHECK(config::sanitize(s).empty());
    CHECK_EQ(s.processName, std::string("RobRun.exe"));
    CHECK(!s.demoMode);
}

TEST(settings, roundtrip_preserves_every_field) {
    Settings a;
    a.processName = "Other.exe"; a.demoMode = true; a.autoStart = true;
    a.overlayMode = OverlayMode::FullScreen; a.overlayVisible = false;
    a.hudX = 55.5f; a.hudY = 66.25f; a.hudW = 410.f; a.hudH = 220.f; a.hudOpacity = 0.5f;
    a.espBox = false; a.maxDistance = 750.5f; a.fovRadius = 222.f; a.boxThickness = 3;
    a.colEnemy[0] = 0.1f; a.colEnemy[3] = 0.75f;
    a.readIntervalMs = 33;
    a.hotkeys.startStop = *keys::vkFromName("F10");
    a.hotkeys.quit = *keys::vkFromName("DELETE");

    Settings b;
    auto r = config::parse(b, config::serialize(a));
    CHECK(r.warnings.empty());
    CHECK_EQ(b.processName, a.processName);
    CHECK(b.demoMode && b.autoStart);
    CHECK(b.overlayMode == OverlayMode::FullScreen);
    CHECK(!b.overlayVisible);
    CHECK_EQ(b.hudX, 55.5f);
    CHECK_EQ(b.hudY, 66.25f);
    CHECK_EQ(b.hudW, 410.f);
    CHECK_EQ(b.hudOpacity, 0.5f);
    CHECK(!b.espBox);
    CHECK_EQ(b.maxDistance, 750.5f);
    CHECK_EQ(b.boxThickness, 3);
    CHECK_EQ(b.colEnemy[0], 0.1f);
    CHECK_EQ(b.colEnemy[3], 0.75f);
    CHECK_EQ(b.readIntervalMs, 33);
    CHECK_EQ(b.hotkeys.startStop, a.hotkeys.startStop);
    CHECK_EQ(b.hotkeys.quit, a.hotkeys.quit);
}

TEST(settings, save_then_load_from_disk) {
    auto dir = tmpDir("save");
    auto path = dir / "sub" / "settings.ini";
    Settings a; a.fovRadius = 321.f; a.demoMode = true;
    std::string err;
    CHECK(config::save(a, path, &err));
    CHECK(fs::exists(path));
    CHECK(!fs::exists(path.string() + ".tmp"));
    Settings b;
    auto r = config::load(b, path);
    CHECK(r.fileFound && r.ok);
    CHECK_EQ(b.fovRadius, 321.f);
    CHECK(b.demoMode);
    // overwrite works
    a.fovRadius = 123.f;
    CHECK(config::save(a, path, &err));
    Settings c; config::load(c, path);
    CHECK_EQ(c.fovRadius, 123.f);
}

TEST(settings, missing_file_reports_not_found_and_keeps_defaults) {
    Settings s;
    auto r = config::load(s, fs::temp_directory_path() / "robrun_definitely_missing" / "x.ini");
    CHECK(!r.fileFound);
    CHECK(r.ok);
    CHECK_EQ(s.processName, std::string("RobRun.exe"));
}

TEST(settings, garbage_values_warn_and_keep_defaults) {
    Settings s;
    auto r = config::parse(s, "esp.max_distance = banana\nesp.box = maybe\nfoo.bar = 1\nno equals here\n"
                              "hotkeys.start_stop = NOTAKEY\n# comment\n; other comment\n");
    CHECK(s.maxDistance == Settings{}.maxDistance);
    CHECK(s.espBox == Settings{}.espBox);
    CHECK_EQ(s.hotkeys.startStop, Settings{}.hotkeys.startStop);
    CHECK(r.warnings.size() >= 5);
}

TEST(settings, out_of_range_values_are_clamped) {
    Settings s;
    auto r = config::parse(s, "perf.read_interval_ms = 0\nesp.max_distance = 99999999\nesp.box_thickness = 99\n"
                              "overlay.hud_opacity = -3\nesp.color_enemy = 5, -1, 0.5, 2\n");
    CHECK_EQ(s.readIntervalMs, 1);
    CHECK_EQ(s.maxDistance, 100000.f);
    CHECK_EQ(s.boxThickness, 8);
    CHECK_EQ(s.hudOpacity, 0.1f);
    CHECK_EQ(s.colEnemy[0], 1.f);
    CHECK_EQ(s.colEnemy[1], 0.f);
    CHECK(!r.warnings.empty());
}

TEST(settings, duplicate_hotkeys_are_repaired) {
    Settings s;
    config::parse(s, "hotkeys.start_stop = F6\nhotkeys.layout_edit = F6\n");
    CHECK(s.hotkeys.startStop != s.hotkeys.layoutEdit);
}

TEST(settings, nan_and_inf_floats_rejected) {
    Settings s;
    auto r = config::parse(s, "esp.fov_radius = nan\nesp.max_distance = inf\n");
    CHECK_EQ(s.fovRadius, Settings{}.fovRadius);
    CHECK_EQ(s.maxDistance, Settings{}.maxDistance);
    CHECK(r.warnings.size() >= 2);
}

TEST(settings, crlf_line_endings_and_whitespace_ok) {
    Settings s;
    config::parse(s, "  general.demo_mode =   true \r\n esp.box_thickness=2\r\n");
    CHECK(s.demoMode);
    CHECK_EQ(s.boxThickness, 2);
}

TEST(settings, hotkey_repair_never_leaves_collisions) {
    Settings s;
    // Every slot set to the same key: must end with six distinct bindable keys.
    config::parse(s, "hotkeys.toggle_menu = F1\nhotkeys.start_stop = F1\nhotkeys.toggle_overlay = F1\n"
                     "hotkeys.layout_edit = F1\nhotkeys.toggle_demo = F1\nhotkeys.quit = F1\n");
    std::vector<int> codes = { s.hotkeys.toggleMenu, s.hotkeys.startStop, s.hotkeys.toggleOverlay,
                               s.hotkeys.layoutEdit, s.hotkeys.toggleDemo, s.hotkeys.quit };
    CHECK(keys::findDuplicates(codes).empty());
    for (int c : codes) CHECK(keys::isBindable(c));
}
