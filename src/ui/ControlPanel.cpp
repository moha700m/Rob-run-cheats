#include "ControlPanel.h"

#include "core/Keys.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

    constexpr ImVec4 kGreen  { 0.30f, 0.85f, 0.50f, 1.f };
    constexpr ImVec4 kAmber  { 1.00f, 0.70f, 0.20f, 1.f };
    constexpr ImVec4 kRed    { 1.00f, 0.40f, 0.40f, 1.f };
    constexpr ImVec4 kGrey   { 0.60f, 0.65f, 0.72f, 1.f };
    constexpr ImVec4 kBlue   { 0.35f, 0.65f, 1.00f, 1.f };

    ImVec4 linkColor(Link l) {
        switch (l) {
        case Link::Live:             return kGreen;
        case Link::Demo:             return kAmber;
        case Link::Error:
        case Link::SdkNotConfigured: return kRed;
        case Link::Waiting:
        case Link::GameDetected:     return kBlue;
        default:                     return kGrey;
        }
    }

    ImVec4 levelColor(LogLevel l) {
        switch (l) {
        case LogLevel::Debug: return kGrey;
        case LogLevel::Info:  return { 0.85f, 0.88f, 0.92f, 1.f };
        case LogLevel::Warn:  return kAmber;
        case LogLevel::Error: return kRed;
        }
        return kGrey;
    }

    void pill(const char* text, ImVec4 color) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        ImGui::Button(text);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    void kv(const char* key, const char* fmt, ImVec4 color, ...) {
        ImGui::TextColored(kGrey, "%s", key);
        ImGui::SameLine(190);
        char buf[512];
        va_list ap; va_start(ap, color); std::vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
        ImGui::TextColored(color, "%s", buf);
    }

    bool containsNoCase(const std::string& hay, const char* needle) {
        if (!needle[0]) return true;
        std::string h = hay, n = needle;
        std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return h.find(n) != std::string::npos;
    }

    void tabDashboard(PanelModel& m, PanelActions& a) {
        const SessionStatus& st = m.status;

        // Primary control row
        const bool running = st.running;
        ImGui::PushStyleColor(ImGuiCol_Button, running ? ImVec4(0.65f, 0.20f, 0.20f, 1.f) : ImVec4(0.15f, 0.55f, 0.35f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, running ? ImVec4(0.78f, 0.25f, 0.25f, 1.f) : ImVec4(0.20f, 0.68f, 0.43f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, running ? ImVec4(0.55f, 0.15f, 0.15f, 1.f) : ImVec4(0.12f, 0.45f, 0.28f, 1.f));
        char label[64];
        std::snprintf(label, sizeof(label), "%s  [%s]", running ? "STOP" : "START", keys::nameFromVk(m.settings->hotkeys.startStop).c_str());
        if (ImGui::Button(label, { 190, 48 })) a.startStop = true;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        pill(toString(st.link), linkColor(st.link));

        ImGui::SameLine();
        bool demo = m.settings->demoMode;
        if (ImGui::Checkbox("Demo Mode (synthetic data)", &demo)) m.settings->demoMode = demo;

        ImGui::Spacing();
        ImGui::SeparatorText("Connection");

        if (st.game.found) kv("Game process", "%s  (PID %u)", kGreen, m.settings->processName.c_str(), st.game.pid);
        else               kv("Game process", "%s  not running", kGrey, m.settings->processName.c_str());
        kv("Data source", "%s", linkColor(st.link),
           st.source == DataSource::Live ? "LIVE - process memory"
           : st.source == DataSource::Demo ? "DEMO - synthetic, not from the game" : "none");
        kv("Status", "%s", linkColor(st.link), st.detail.empty() ? "-" : st.detail.c_str());
        kv("Entities", "%u", ImGui::GetStyleColorVec4(ImGuiCol_Text), st.entityCount);
        kv("Updates", "%.0f/s   ok %llu   failed %llu", ImGui::GetStyleColorVec4(ImGuiCol_Text),
           st.updatesPerSec, static_cast<unsigned long long>(st.updates), static_cast<unsigned long long>(st.failures));

        ImGui::Spacing();
        ImGui::SeparatorText("Offsets (real game link)");
        if (!m.offsetsFileFound) {
            kv("offsets file", "not found: %s", kAmber, m.offsetsPath.c_str());
        } else {
            kv("offsets file", "%s", kGreen, m.offsetsPath.c_str());
        }
        if (!m.buildLabel.empty()) kv("build label", "%s", ImGui::GetStyleColorVec4(ImGuiCol_Text), m.buildLabel.c_str());
        if (m.offsetsMissing.empty()) {
            kv("required values", "complete", kGreen);
        } else {
            std::string list;
            for (size_t i = 0; i < m.offsetsMissing.size(); ++i) list += (i ? ", " : "") + m.offsetsMissing[i];
            kv("required values", "MISSING: %s", kRed, list.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, kGrey);
            ImGui::TextWrapped("Real ESP is unavailable until offsets.ini contains values dumped from your own RobRun build "
                               "(see docs/OFFSETS.md). Demo Mode works without them.");
            ImGui::PopStyleColor();
        }
        for (const auto& w : m.offsetsWarnings) ImGui::TextColored(kAmber, "warning: %s", w.c_str());
        if (ImGui::Button("Reload offsets")) a.reloadOffsets = true;
        ImGui::SameLine();
        if (ImGui::Button("Create offsets.template.ini")) a.writeOffsetsTemplate = true;

        ImGui::Spacing();
        ImGui::SeparatorText("Overlay");
        kv("Overlay window", "%s", m.overlayShown ? kGreen : kGrey,
           m.overlayShown ? (m.layoutEditing ? "visible - layout edit mode" : "visible, click-through") : "hidden");
        kv("Renderer", "%s   %.0f FPS", ImGui::GetStyleColorVec4(ImGuiCol_Text), m.renderer.c_str(), m.fps);
        if (ImGui::Button(m.settings->overlayVisible ? "Hide overlay" : "Show overlay")) a.toggleOverlay = true;
        ImGui::SameLine();
        if (ImGui::Button(m.layoutEditing ? "Finish layout edit" : "Edit HUD layout")) a.toggleLayoutEdit = true;
    }

    void tabLogs(PanelModel& m, PanelUiState& ui, PanelActions& a) {
        if (m.logger) {
            auto fresh = m.logger->since(ui.lastLogId);
            for (auto& e : fresh) { ui.lastLogId = e.id; ui.logCache.push_back(std::move(e)); }
            if (ui.logCache.size() > 2000) ui.logCache.erase(ui.logCache.begin(), ui.logCache.begin() + (ui.logCache.size() - 2000));
        }

        ImGui::SetNextItemWidth(110);
        const char* levels[] = { "Debug+", "Info+", "Warn+", "Error" };
        ImGui::Combo("##lvl", &ui.minLevel, levels, 4);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        ImGui::InputTextWithHint("##flt", "filter text", ui.filter, sizeof(ui.filter));
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &ui.autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { ui.logCache.clear(); if (m.logger) m.logger->clear(); }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string all;
            for (const auto& e : ui.logCache)
                if (static_cast<int>(e.level) >= ui.minLevel && containsNoCase(e.text, ui.filter)) all += Logger::format(e) + "\n";
            ImGui::SetClipboardText(all.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Open log file")) a.openLogFile = true;

        ImGui::BeginChild("##log", { 0, 0 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : ui.logCache) {
            if (static_cast<int>(e.level) < ui.minLevel || !containsNoCase(e.text, ui.filter)) continue;
            ImGui::TextColored(levelColor(e.level), "%s", Logger::format(e).c_str());
        }
        if (ui.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    void hotkeyRow(PanelModel& m, PanelActions& a, int slot, const char* label, int vk) {
        ImGui::PushID(slot);
        ImGui::Text("%s", label);
        ImGui::SameLine(240);
        const bool capturing = (m.capturingSlot == slot);
        char btn[64];
        if (capturing) std::snprintf(btn, sizeof(btn), "press a key...  (Esc = cancel)");
        else           std::snprintf(btn, sizeof(btn), "%s", keys::nameFromVk(vk).c_str());
        if (ImGui::Button(btn, { 230, 0 })) a.rebindSlot = capturing ? -2 : slot;
        ImGui::PopID();
    }

    void tabSettings(PanelModel& m, PanelActions& a) {
        Settings& s = *m.settings;

        if (ImGui::Button("Save now")) a.saveSettings = true;
        ImGui::SameLine();
        if (ImGui::Button("Reload from file")) a.reloadSettings = true;
        ImGui::SameLine();
        if (ImGui::Button("Reset to defaults")) a.resetDefaults = true;
        ImGui::SameLine();
        if (ImGui::Button("Open folder")) a.openFolder = true;
        ImGui::SameLine();
        ImGui::TextColored(m.settingsDirty ? kAmber : kGrey, "%s", m.settingsDirty ? "unsaved changes (saved automatically on exit)" : "saved");
        ImGui::TextColored(kGrey, "%s", m.settingsPath.c_str());
        ImGui::Separator();

        ImGui::BeginChild("##settings_scroll", { 0, 0 }, ImGuiChildFlags_None);

        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            char proc[130]; std::snprintf(proc, sizeof(proc), "%s", s.processName.c_str());
            if (ImGui::InputText("Game process name", proc, sizeof(proc))) s.processName = proc;
            char off[260]; std::snprintf(off, sizeof(off), "%s", s.offsetsFile.c_str());
            if (ImGui::InputText("Offsets file", off, sizeof(off))) s.offsetsFile = off;
            ImGui::Checkbox("Demo Mode", &s.demoMode);
            ImGui::Checkbox("Start automatically when the app opens", &s.autoStart);
        }

        if (ImGui::CollapsingHeader("Hotkeys (global)", ImGuiTreeNodeFlags_DefaultOpen)) {
            hotkeyRow(m, a, 0, "Show / hide this window", s.hotkeys.toggleMenu);
            hotkeyRow(m, a, 1, "Start / stop",            s.hotkeys.startStop);
            hotkeyRow(m, a, 2, "Show / hide overlay",     s.hotkeys.toggleOverlay);
            hotkeyRow(m, a, 3, "Edit HUD layout",         s.hotkeys.layoutEdit);
            hotkeyRow(m, a, 4, "Toggle Demo Mode",        s.hotkeys.toggleDemo);
            hotkeyRow(m, a, 5, "Quit",                    s.hotkeys.quit);
        }

        if (ImGui::CollapsingHeader("Overlay")) {
            int mode = static_cast<int>(s.overlayMode);
            const char* modes[] = { "Fit game window", "Full screen (monitor)" };
            if (ImGui::Combo("Display mode", &mode, modes, 2)) s.overlayMode = static_cast<OverlayMode>(mode);
            ImGui::Checkbox("Overlay enabled", &s.overlayVisible);
            ImGui::Checkbox("Only show while the game is the active window", &s.onlyWhenGameForeground);
            ImGui::Checkbox("Show HUD panel", &s.showHud);
            ImGui::SliderFloat("HUD opacity", &s.hudOpacity, 0.1f, 1.0f);
            ImGui::TextColored(kGrey, "HUD position/size: %.0f,%.0f  %.0fx%.0f  (use \"Edit HUD layout\")", s.hudX, s.hudY, s.hudW, s.hudH);
        }

        if (ImGui::CollapsingHeader("ESP")) {
            ImGui::Checkbox("ESP enabled", &s.espEnabled);
            ImGui::Checkbox("Box", &s.espBox);               ImGui::SameLine(200);
            ImGui::Checkbox("Name", &s.espName);
            ImGui::Checkbox("Health bar", &s.espHealthBar);  ImGui::SameLine(200);
            ImGui::Checkbox("Distance", &s.espDistance);
            ImGui::Checkbox("Snapline", &s.espSnapline);     ImGui::SameLine(200);
            ImGui::Checkbox("Hide teammates", &s.espTeamCheck);
            ImGui::BeginDisabled();
            ImGui::Checkbox("Skeleton (not implemented)", &s.espSkeleton);
            ImGui::EndDisabled();
            ImGui::SliderInt("Box thickness", &s.boxThickness, 1, 8);
            ImGui::SliderFloat("Max distance", &s.maxDistance, 1.0f, 2000.0f);
            ImGui::SliderFloat("Min distance", &s.minDistance, 0.0f, 500.0f);
            ImGui::Checkbox("Crosshair", &s.drawCrosshair);  ImGui::SameLine(200);
            ImGui::Checkbox("FOV circle", &s.drawFovCircle);
            ImGui::SliderFloat("FOV radius (px)", &s.fovRadius, 20.0f, 1000.0f);
            ImGui::ColorEdit4("Enemy", s.colEnemy, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Team", s.colTeam, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Text", s.colText, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Shadow", s.colShadow, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Crosshair colour", s.colCrosshair, ImGuiColorEditFlags_NoInputs);
        }

        if (ImGui::CollapsingHeader("Performance")) {
            ImGui::SliderInt("Read interval (ms)", &s.readIntervalMs, 1, 200);
            ImGui::TextColored(kGrey, "Higher = fewer memory reads and less CPU. Overlay rendering is v-synced.");
        }
        ImGui::EndChild();
    }

    void tabAbout() {
        ImGui::TextUnformatted("RobRun External Overlay");
        ImGui::TextColored(kGrey, "Read-only external overlay: no injection, no hooks, no memory writes,");
        ImGui::TextColored(kGrey, "no anti-cheat bypass, no process hiding.");
        ImGui::Spacing();
        ImGui::TextWrapped("Real game data requires offsets.ini values dumped from your own RobRun build; this project ships none "
                           "and never guesses them. Without them the app can only run Demo Mode, which is always labelled as synthetic.");
        ImGui::Spacing();
        ImGui::TextColored(kGrey, "Dear ImGui %s", IMGUI_VERSION);
    }

} // namespace

void applyControlTheme() {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark(&st);
    st.WindowRounding = 0; st.FrameRounding = 6; st.GrabRounding = 6; st.ChildRounding = 6; st.TabRounding = 6;
    st.FramePadding = { 10, 6 }; st.ItemSpacing = { 10, 8 }; st.WindowPadding = { 16, 14 };
    st.FrameBorderSize = 0; st.ScrollbarRounding = 8;
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]       = { 0.059f, 0.067f, 0.090f, 1.f };
    c[ImGuiCol_ChildBg]        = { 0.075f, 0.086f, 0.115f, 1.f };
    c[ImGuiCol_FrameBg]        = { 0.110f, 0.125f, 0.165f, 1.f };
    c[ImGuiCol_FrameBgHovered] = { 0.150f, 0.170f, 0.220f, 1.f };
    c[ImGuiCol_Button]         = { 0.145f, 0.200f, 0.320f, 1.f };
    c[ImGuiCol_ButtonHovered]  = { 0.200f, 0.280f, 0.450f, 1.f };
    c[ImGuiCol_ButtonActive]   = { 0.120f, 0.170f, 0.280f, 1.f };
    c[ImGuiCol_Header]         = { 0.145f, 0.200f, 0.320f, 1.f };
    c[ImGuiCol_Tab]            = { 0.090f, 0.105f, 0.145f, 1.f };
    c[ImGuiCol_TabHovered]     = { 0.200f, 0.280f, 0.450f, 1.f };
    c[ImGuiCol_TabSelected]    = { 0.160f, 0.250f, 0.420f, 1.f };
    c[ImGuiCol_CheckMark]      = { 0.35f, 0.75f, 1.00f, 1.f };
    c[ImGuiCol_SliderGrab]     = { 0.35f, 0.65f, 1.00f, 1.f };
}

PanelActions drawControlPanel(PanelModel& m, PanelUiState& ui, float width, float height) {
    PanelActions a;
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ width, height });
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::TextColored({ 0.75f, 0.85f, 1.f, 1.f }, "ROBRUN  /  EXTERNAL OVERLAY");
    ImGui::SameLine(ImGui::GetWindowWidth() - 270);
    if (ImGui::SmallButton("Hide window")) a.hideWindow = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Quit")) a.quit = true;
    ImGui::Separator();

    if (m.status.link == Link::Demo) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.35f, 0.22f, 0.04f, 1.f });
        ImGui::BeginChild("##demo", { 0, 34 }, ImGuiChildFlags_Borders);
        ImGui::TextColored(kAmber, "DEMO MODE: everything on the overlay is synthetic. Nothing is read from the game.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Dashboard")) { tabDashboard(m, a); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Logs"))      { tabLogs(m, ui, a);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Settings"))  { tabSettings(m, a);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("About"))     { tabAbout();         ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    // Footer status line
    ImGui::SetCursorPosY(height - 28);
    ImGui::Separator();
    ImGui::TextColored(kGrey, "%s", m.statusLine.empty() ? "Ready" : m.statusLine.c_str());

    ImGui::End();
    return a;
}
