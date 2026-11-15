// Settings.h - All user-editable application settings, with load/save to a
// plain "key = value" file (see docs/CONFIG.md). No OS dependencies, so it is
// covered by the portable unit tests.
#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class OverlayMode : int {
    FitGameWindow = 0,   // overlay follows the game window's client area
    FullScreen    = 1,   // overlay covers the primary monitor
};

struct Hotkeys {
    int toggleMenu    = 0x2D;   // INSERT  : show/hide this control window
    int startStop     = 0x77;   // F8      : start / stop
    int toggleOverlay = 0x76;   // F7      : show/hide the overlay
    int layoutEdit    = 0x75;   // F6      : move/resize the overlay HUD
    int toggleDemo    = 0x78;   // F9      : Demo Mode on/off
    int quit          = 0x23;   // END     : quit
};

struct Settings {
    int version = 1;

    // ---- General ----
    std::string processName = "RobRun.exe";
    std::string offsetsFile = "offsets.ini";   // relative paths resolve next to the .exe
    bool demoMode  = false;
    bool autoStart = false;

    // ---- Overlay ----
    OverlayMode overlayMode = OverlayMode::FitGameWindow;
    bool  overlayVisible        = true;
    bool  onlyWhenGameForeground = true;   // hide overlay while another app has focus
    bool  showHud               = true;
    float hudX = 20.f, hudY = 20.f, hudW = 300.f, hudH = 170.f;
    float hudOpacity            = 0.85f;

    // ---- ESP ----
    bool  espEnabled      = true;
    bool  espBox          = true;
    bool  espName         = true;
    bool  espHealthBar    = true;
    bool  espDistance     = true;
    bool  espSnapline     = false;
    bool  espSkeleton     = false;   // reserved: needs bone data, not implemented
    bool  espTeamCheck    = true;    // hide teammates
    float maxDistance     = 300.0f;  // world units
    float minDistance     = 0.0f;
    bool  drawCrosshair   = false;
    bool  drawFovCircle   = false;
    float fovRadius       = 150.0f;  // pixels
    int   boxThickness    = 1;

    float colEnemy[4]     = { 1.00f, 0.20f, 0.20f, 1.00f };
    float colTeam[4]      = { 0.20f, 0.60f, 1.00f, 1.00f };
    float colText[4]      = { 1.00f, 1.00f, 1.00f, 1.00f };
    float colShadow[4]    = { 0.00f, 0.00f, 0.00f, 0.80f };
    float colCrosshair[4] = { 0.00f, 1.00f, 0.60f, 1.00f };

    // ---- Performance ----
    int readIntervalMs = 16;   // ms between world refreshes (1..1000)

    // ---- Control window ----
    int mainWidth = 980, mainHeight = 640;

    Hotkeys hotkeys;
};

// Calls v.<type>("section.key", field) for every persisted field. Used by both
// the reader and the writer so they can never drift apart.
template <class S, class V>
void visitSettings(S& s, V& v) {
    v.str ("general.process_name", s.processName);
    v.str ("general.offsets_file", s.offsetsFile);
    v.flag("general.demo_mode",    s.demoMode);
    v.flag("general.auto_start",   s.autoStart);

    v.enum_("overlay.mode",                  s.overlayMode);
    v.flag ("overlay.visible",               s.overlayVisible);
    v.flag ("overlay.only_when_game_foreground", s.onlyWhenGameForeground);
    v.flag ("overlay.show_hud",              s.showHud);
    v.real ("overlay.hud_x",                 s.hudX);
    v.real ("overlay.hud_y",                 s.hudY);
    v.real ("overlay.hud_w",                 s.hudW);
    v.real ("overlay.hud_h",                 s.hudH);
    v.real ("overlay.hud_opacity",           s.hudOpacity);

    v.flag("esp.enabled",      s.espEnabled);
    v.flag("esp.box",          s.espBox);
    v.flag("esp.name",         s.espName);
    v.flag("esp.health_bar",   s.espHealthBar);
    v.flag("esp.distance",     s.espDistance);
    v.flag("esp.snapline",     s.espSnapline);
    v.flag("esp.skeleton",     s.espSkeleton);
    v.flag("esp.team_check",   s.espTeamCheck);
    v.real("esp.max_distance", s.maxDistance);
    v.real("esp.min_distance", s.minDistance);
    v.flag("esp.crosshair",    s.drawCrosshair);
    v.flag("esp.fov_circle",   s.drawFovCircle);
    v.real("esp.fov_radius",   s.fovRadius);
    v.integer("esp.box_thickness", s.boxThickness);
    v.color("esp.color_enemy",     s.colEnemy);
    v.color("esp.color_team",      s.colTeam);
    v.color("esp.color_text",      s.colText);
    v.color("esp.color_shadow",    s.colShadow);
    v.color("esp.color_crosshair", s.colCrosshair);

    v.integer("perf.read_interval_ms", s.readIntervalMs);

    v.integer("window.main_width",  s.mainWidth);
    v.integer("window.main_height", s.mainHeight);

    v.key("hotkeys.toggle_menu",    s.hotkeys.toggleMenu);
    v.key("hotkeys.start_stop",     s.hotkeys.startStop);
    v.key("hotkeys.toggle_overlay", s.hotkeys.toggleOverlay);
    v.key("hotkeys.layout_edit",    s.hotkeys.layoutEdit);
    v.key("hotkeys.toggle_demo",    s.hotkeys.toggleDemo);
    v.key("hotkeys.quit",           s.hotkeys.quit);
}

namespace config {

    struct LoadResult {
        bool fileFound = false;
        bool ok        = true;                  // false only for I/O errors
        std::vector<std::string> warnings;      // bad values, unknown keys, ...
    };

    // Clamps every value into its valid range and repairs duplicate hotkeys.
    // Returns human-readable notes about anything it changed.
    std::vector<std::string> sanitize(Settings& s);

    LoadResult parse(Settings& s, std::string_view text);
    LoadResult load(Settings& s, const std::filesystem::path& path);

    std::string serialize(const Settings& s);
    // Writes to "<path>.tmp" then renames over `path` so a crash never leaves a
    // half-written file.
    bool save(const Settings& s, const std::filesystem::path& path, std::string* error = nullptr);

} // namespace config
