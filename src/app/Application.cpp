#include "Application.h"

#include "logging/Logger.h"
#include "ui/EspRenderer.h"

#include "imgui.h"

#include <shellapi.h>

#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// ---------------------------------------------------------------- CLI -------

std::string usageText() {
    return
        "RobRunOverlay.exe [options]\n"
        "  --config <file>   settings file (default: RobRunOverlay.ini next to the exe)\n"
        "  --log <file>      log file (default: RobRunOverlay.log next to the settings file)\n"
        "  --demo            enable Demo Mode (synthetic data)\n"
        "  --autostart       start immediately\n"
        "  --selftest        run the automated GUI/runtime self test and exit (0 = pass)\n"
        "  --report <file>   where --selftest writes its report (default: selftest_report.txt)\n"
        "  --help            this text\n";
}

bool parseCommandLine(int argc, wchar_t** argv, CliOptions& o, std::string& error) {
    auto need = [&](int& i, const wchar_t* name, fs::path& dst) {
        if (i + 1 >= argc) { error = std::string("missing value for ") + std::string(name, name + wcslen(name)); return false; }
        dst = argv[++i];
        return true;
    };
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--selftest")       o.selfTest = true;
        else if (a == L"--demo")      o.demo = true;
        else if (a == L"--autostart") o.autoStart = true;
        else if (a == L"--help" || a == L"-h" || a == L"/?") o.help = true;
        else if (a == L"--config") { if (!need(i, L"--config", o.configPath)) return false; }
        else if (a == L"--log")    { if (!need(i, L"--log", o.logPath)) return false; }
        else if (a == L"--report") { if (!need(i, L"--report", o.reportPath)) return false; }
        else { error = "unknown option: " + std::string(a.begin(), a.end()); return false; }
    }
    return true;
}

// ------------------------------------------------------------ helpers -------

namespace {

    std::wstring widen(const std::string& s) {
        std::wstring w;
        for (unsigned char c : s) w.push_back(c);   // process names are ASCII
        return w;
    }

    bool keyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

    struct GuiResources { DWORD gdi = 0, user = 0, handles = 0; };
    GuiResources measureResources() {
        GuiResources r;
        r.gdi  = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        r.user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
        GetProcessHandleCount(GetCurrentProcess(), &r.handles);
        return r;
    }

    void writeStdout(const std::string& s) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN) {
            DWORD n = 0;
            WriteFile(h, s.data(), static_cast<DWORD>(s.size()), &n, nullptr);
        }
    }

} // namespace

fs::path Application::exeDir() {
    wchar_t buf[MAX_PATH * 2]{};
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(_countof(buf)));
    if (n == 0 || n >= _countof(buf)) return fs::current_path();
    return fs::path(buf).parent_path();
}

bool Application::dirWritable(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path probe = dir / ".robrun_write_test";
    std::ofstream f(probe, std::ios::binary | std::ios::trunc);
    const bool ok = static_cast<bool>(f);
    f.close();
    fs::remove(probe, ec);
    return ok;
}

// -------------------------------------------------------- construction ------

Application::Application(CliOptions opts)
    : opts_(std::move(opts)), keys_([](int vk) { return keyDown(vk); }) {}

Application::~Application() { shutdown(); }

bool Application::initCommon(std::string& error) {
    if (commonReady_) return true;
    exeDir_ = exeDir();

    // Settings / log location: explicit, else next to the exe, else %APPDATA%.
    fs::path dataDir = exeDir_;
    if (!opts_.configPath.empty()) {
        settingsPath_ = fs::absolute(opts_.configPath);
        dataDir = settingsPath_.parent_path();
    } else {
        if (!dirWritable(exeDir_)) {
            wchar_t appdata[MAX_PATH]{};
            if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) dataDir = fs::path(appdata) / "RobRunOverlay";
        }
        settingsPath_ = dataDir / "RobRunOverlay.ini";
    }
    logPath_ = opts_.logPath.empty() ? dataDir / "RobRunOverlay.log" : fs::absolute(opts_.logPath);

    std::string logErr;
    if (!Logger::global().openFile(logPath_, &logErr)) logWarn("{} (continuing without file log)", logErr);
    logInfo("RobRun Overlay starting (settings: {}, log: {})", settingsPath_.string(), logPath_.string());

    // Settings
    const auto lr = config::load(settings_, settingsPath_);
    if (!lr.fileFound) logInfo("no settings file yet - using defaults");
    else if (!lr.ok) logWarn("settings file could not be read - using defaults");
    for (const auto& w : lr.warnings) logWarn("settings: {}", w);
    if (opts_.demo) settings_.demoMode = true;
    savedText_ = config::serialize(settings_);

    // Providers + worker
    worker_ = std::make_unique<DataWorker>(live_, demo_);
    reloadOffsets();
    syncWorker();

    commonReady_ = true;
    (void)error;
    return true;
}

void Application::reloadOffsets() {
    offsets_ = SdkOffsets{};
    fs::path p = fs::path(settings_.offsetsFile);
    if (p.is_relative()) p = exeDir_ / p;
    offsetsPath_ = p;
    const auto r = offsets::load(offsets_, offsetsPath_);
    offsetsFound_ = r.fileFound;
    offsetsWarnings_ = r.warnings;
    if (!r.fileFound) logWarn("offsets file not found: {} - real game data is unavailable (Demo Mode still works)", offsetsPath_.string());
    else logInfo("offsets loaded from {} ({})", offsetsPath_.string(), offsets_.ready() ? "complete" : "INCOMPLETE");
    for (const auto& w : r.warnings) logWarn("offsets: {}", w);
    if (!offsets_.buildLabel.empty()) logInfo("offsets build label: {}", offsets_.buildLabel);
    applyProviderConfig();
}

void Application::applyProviderConfig() {
    lastProcessName_ = widen(settings_.processName);
    live_.configure(lastProcessName_, offsets_);
}

void Application::syncWorker() {
    if (!worker_) return;
    worker_->setIntervalMs(settings_.readIntervalMs);
    worker_->setDemo(settings_.demoMode);
    if (lastProcessName_ != widen(settings_.processName)) applyProviderConfig();
}

void Application::shutdown() {
    if (!commonReady_ && !worker_) return;
    const auto t0 = Clock::now();
    if (worker_) {
        worker_->stop();
        worker_->shutdown();
        worker_.reset();
    }
    destroyWindows();
    if (commonReady_) {
        refreshDirty();
        if (dirty_ && !opts_.selfTest) saveSettings();
        logInfo("shutdown complete in {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());
        Logger::global().closeFile();
    }
    commonReady_ = false;
}

bool Application::createWindows(std::string& error) {
    if (!ctl_.create(settings_.mainWidth, settings_.mainHeight, error)) return false;
    if (!ovl_.create(error)) { ctl_.destroy(); return false; }
    return true;
}

void Application::destroyWindows() {
    ovl_.destroy();
    ctl_.destroy();
    gameHwnd_ = nullptr;
}

// ------------------------------------------------------------- loop ---------

bool Application::pumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

void Application::setStatus(const std::string& s) {
    statusLine_ = s;
    statusSetAt_ = Clock::now();
    logInfo("{}", s);
}

void Application::toggleRun() {
    if (!worker_) return;
    if (worker_->running()) {
        worker_->stop();
        if (layoutEdit_) setLayoutEdit(false);
        setStatus("Stopped");
    } else {
        worker_->start();
        setStatus(settings_.demoMode ? "Started (Demo Mode)" : "Started");
    }
}

void Application::setLayoutEdit(bool on) {
    layoutEdit_ = on;
    ovl_.setInteractive(on);
    setStatus(on ? "HUD layout edit: drag/resize the panel on the overlay" : "HUD layout saved");
}

bool Application::saveSettings() {
    std::string err;
    if (!config::save(settings_, settingsPath_, &err)) {
        logError("saving settings failed: {}", err);
        setStatus("Saving settings FAILED: " + err);
        return false;
    }
    savedText_ = config::serialize(settings_);
    dirty_ = false;
    logInfo("settings saved to {}", settingsPath_.string());
    return true;
}

bool Application::reloadSettings() {
    Settings fresh;
    const auto r = config::load(fresh, settingsPath_);
    if (!r.fileFound) { setStatus("No settings file to reload yet"); return false; }
    for (const auto& w : r.warnings) logWarn("settings: {}", w);
    settings_ = fresh;
    savedText_ = config::serialize(settings_);
    dirty_ = false;
    reloadOffsets();
    syncWorker();
    setStatus("Settings reloaded");
    return true;
}

void Application::refreshDirty() {
    dirty_ = (config::serialize(settings_) != savedText_);
}

void Application::pollHotkeys() {
    if (capturingSlot_ >= 0) {
        if (keys_.pressed(0x1B)) { capturingSlot_ = -1; setStatus("Hotkey change cancelled"); return; }
        for (int vk = 0x08; vk <= 0xFE; ++vk) {
            if (!keys_.pressed(vk)) continue;
            if (!keys::isBindable(vk)) continue;
            int* slots[] = { &settings_.hotkeys.toggleMenu, &settings_.hotkeys.startStop, &settings_.hotkeys.toggleOverlay,
                             &settings_.hotkeys.layoutEdit, &settings_.hotkeys.toggleDemo, &settings_.hotkeys.quit };
            bool clash = false;
            for (int i = 0; i < 6; ++i) if (i != capturingSlot_ && *slots[i] == vk) clash = true;
            if (clash) { setStatus("Key " + keys::nameFromVk(vk) + " is already used by another hotkey"); }
            else { *slots[capturingSlot_] = vk; setStatus("Hotkey set to " + keys::nameFromVk(vk)); }
            capturingSlot_ = -1;
            break;
        }
        return;
    }

    const Hotkeys& h = settings_.hotkeys;
    if (keys_.pressed(h.toggleMenu)) {
        if (ctl_.shown()) ctl_.hideWindow(); else ctl_.showWindow();
    }
    if (keys_.pressed(h.startStop))     toggleRun();
    if (keys_.pressed(h.toggleOverlay)) { settings_.overlayVisible = !settings_.overlayVisible; setStatus(settings_.overlayVisible ? "Overlay enabled" : "Overlay disabled"); }
    if (keys_.pressed(h.layoutEdit))    setLayoutEdit(!layoutEdit_);
    if (keys_.pressed(h.toggleDemo))    { settings_.demoMode = !settings_.demoMode; setStatus(settings_.demoMode ? "Demo Mode ON" : "Demo Mode OFF"); }
    if (keys_.pressed(h.quit))          quitRequested_ = true;
}

void Application::handleActions(const PanelActions& a) {
    if (a.startStop) toggleRun();
    if (a.saveSettings && saveSettings()) setStatus("Settings saved");
    if (a.reloadSettings) reloadSettings();
    if (a.resetDefaults) { settings_ = Settings{}; syncWorker(); reloadOffsets(); setStatus("Settings reset to defaults (not saved yet)"); }
    if (a.reloadOffsets) { reloadOffsets(); setStatus(offsetsFound_ ? "Offsets reloaded" : "Offsets file not found"); }
    if (a.writeOffsetsTemplate) {
        const fs::path p = exeDir_ / "offsets.template.ini";
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f << offsets::templateText();
        setStatus(f ? "Wrote " + p.string() : "Could not write " + p.string());
    }
    if (a.toggleLayoutEdit) setLayoutEdit(!layoutEdit_);
    if (a.toggleOverlay) { settings_.overlayVisible = !settings_.overlayVisible; }
    if (a.hideWindow) { ctl_.hideWindow(); setStatus("Window hidden - press " + keys::nameFromVk(settings_.hotkeys.toggleMenu) + " to show it again"); }
    if (a.quit) quitRequested_ = true;
    if (a.openLogFile) ShellExecuteW(nullptr, L"open", logPath_.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (a.openFolder) ShellExecuteW(nullptr, L"open", settingsPath_.parent_path().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (a.rebindSlot == -2) { capturingSlot_ = -1; }
    else if (a.rebindSlot >= 0) { capturingSlot_ = a.rebindSlot; keys_.reset(); setStatus("Press the new key (Esc to cancel)"); }
}

void Application::updateOverlay(const SessionStatus& st) {
    if (!ovl_.isOpen()) return;

    // Locate the game's window (cached, re-checked twice a second).
    const auto now = Clock::now();
    if (!st.game.found) { gameHwnd_ = nullptr; gamePid_ = 0; }
    else if (st.game.pid != gamePid_ || !gameHwnd_ || !IsWindow(gameHwnd_) || now - lastWindowSearch_ > 500ms) {
        if (st.game.pid != gamePid_ || !gameHwnd_ || !IsWindow(gameHwnd_)) gameHwnd_ = Overlay::findMainWindow(st.game.pid);
        gamePid_ = st.game.pid;
        lastWindowSearch_ = now;
    }

    const bool demoNoGame = (st.link == Link::Demo) && !gameHwnd_;
    bool want = layoutEdit_ || (settings_.overlayVisible && st.running && (gameHwnd_ || demoNoGame));

    if (want && !layoutEdit_ && settings_.onlyWhenGameForeground && gameHwnd_) {
        HWND fg = GetForegroundWindow();
        HWND root = fg ? GetAncestor(fg, GA_ROOT) : nullptr;
        want = (root == gameHwnd_ || root == ovl_.hwnd() || root == ctl_.hwnd() || fg == gameHwnd_);
    }
    if (want && gameHwnd_ && IsIconic(gameHwnd_)) want = false;

    ovl_.setVisible(want);
    if (want && now - lastPlace_ >= 100ms) {
        ovl_.place(gameHwnd_, settings_.overlayMode);
        lastPlace_ = now;
    }
    if (ovl_.interactive() != layoutEdit_) ovl_.setInteractive(layoutEdit_);
}

void Application::renderControl(const SessionStatus& st) {
    if (!ctl_.shown()) return;

    PanelModel m;
    m.settings = &settings_;
    m.status = st;
    m.logger = &Logger::global();
    m.offsetsMissing = offsets_.missingRequired();
    m.offsetsWarnings = offsetsWarnings_;
    m.offsetsPath = offsetsPath_.string();
    m.offsetsFileFound = offsetsFound_;
    m.buildLabel = offsets_.buildLabel;
    m.settingsPath = settingsPath_.string();
    m.settingsDirty = dirty_;
    m.overlayShown = ovl_.visible();
    m.layoutEditing = layoutEdit_;
    m.capturingSlot = capturingSlot_;
    m.renderer = ctl_.driverName();
    m.statusLine = (Clock::now() - statusSetAt_ < 8s) ? statusLine_ : std::string();

    PanelActions actions;
    ctl_.renderFrame([&] {
        m.fps = ImGui::GetIO().Framerate;
        RECT rc{};
        GetClientRect(ctl_.hwnd(), &rc);
        actions = drawControlPanel(m, panelUi_, static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top));
    });
    handleActions(actions);
}

void Application::renderOverlay(const SessionStatus& st) {
    if (!ovl_.visible()) return;
    const WorldSnap snap = worker_->snapshot();
    ovl_.renderFrame([&] {
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        if (ds.y > 0) demo_.setAspect(ds.x / ds.y);
        esp::renderEsp(snap, settings_, ds.x, ds.y);
        esp::renderHud(st, settings_, layoutEdit_, ImGui::GetIO().Framerate, ds.x, ds.y);
    });
}

void Application::frame() {
    syncWorker();
    pollHotkeys();

    const auto now = Clock::now();
    if (now - lastDirtyCheck_ >= 500ms) { refreshDirty(); lastDirtyCheck_ = now; }

    const SessionStatus st = worker_->status();
    updateOverlay(st);
    renderControl(st);
    renderOverlay(st);

    if (ctl_.closeRequested()) { quitRequested_ = true; ctl_.clearCloseRequest(); }
}

int Application::run() {
    std::string err;
    if (!initCommon(err)) return 1;
    if (!createWindows(err)) {
        logError("cannot create windows: {}", err);
        MessageBoxA(nullptr, ("Cannot start the user interface:\n" + err).c_str(), "RobRun Overlay", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (opts_.autoStart || settings_.autoStart) toggleRun();

    while (!quitRequested_) {
        const auto t0 = Clock::now();
        if (!pumpMessages()) break;
        frame();

        // Pace the loop: ~60 FPS while the overlay is up, ~30 FPS for the window alone,
        // and mostly asleep when nothing is visible.
        const auto budget = ovl_.visible() ? 16ms : (ctl_.shown() ? 33ms : 50ms);
        const auto spent = Clock::now() - t0;
        if (spent < budget) {
            const DWORD ms = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(budget - spent).count());
            MsgWaitForMultipleObjects(0, nullptr, FALSE, ms, QS_ALLINPUT);
        }
    }

    // Remember the window size for next time.
    RECT rc{};
    if (ctl_.isOpen() && !IsIconic(ctl_.hwnd()) && GetClientRect(ctl_.hwnd(), &rc)) {
        settings_.mainWidth = rc.right - rc.left;
        settings_.mainHeight = rc.bottom - rc.top;
        config::sanitize(settings_);
    }
    shutdown();
    return 0;
}

// ---------------------------------------------------------- self test -------

int Application::runSelfTest() {
    struct Result { std::string name; bool ok; std::string detail; };
    std::vector<Result> results;
    auto check = [&](const std::string& name, bool ok, const std::string& detail = {}) {
        results.push_back({ name, ok, detail });
        writeStdout(std::string(ok ? "[ PASS ] " : "[ FAIL ] ") + name + (detail.empty() ? "" : "  (" + detail + ")") + "\n");
    };

    // Isolate from the user's real files.
    const fs::path tmp = fs::temp_directory_path() / ("robrun_selftest_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    opts_.configPath = tmp / "settings.ini";
    opts_.logPath = tmp / "selftest.log";
    opts_.demo = true;

    std::string err;
    check("startup: settings/log/offsets/worker initialise", initCommon(err), err);
    if (!commonReady_) goto finish;

    settings_.demoMode = true;
    settings_.onlyWhenGameForeground = false;
    settings_.processName = "RobRun_selftest_absent.exe";   // guaranteed not running
    applyProviderConfig();
    syncWorker();

    {
        const GuiResources base = measureResources();
        GuiResources afterCycle[2] = {};

        for (int cycle = 0; cycle < 2; ++cycle) {
            const std::string tag = "cycle " + std::to_string(cycle + 1) + ": ";
            const bool created = createWindows(err);
            check(tag + "GUI: control window + overlay created (D3D11 + ImGui)", created, err);
            if (!created) goto finish;
            check(tag + "renderer initialised", !ctl_.driverName().empty(),
                  "control=" + ctl_.driverName() + " overlay=" + ovl_.driverName());

            worker_->start();
            const auto t0 = Clock::now();
            int frames = 0;
            while (Clock::now() - t0 < 700ms || frames < 30) {
                if (!pumpMessages()) break;
                frame();
                ++frames;
                Sleep(4);
            }

            const SessionStatus st = worker_->status();
            check(tag + "Demo Mode: worker publishes demo data", st.link == Link::Demo && st.source == DataSource::Demo &&
                  st.entityCount == static_cast<uint32_t>(DemoProvider::kEntityCount),
                  std::string(toString(st.link)) + ", entities=" + std::to_string(st.entityCount));
            check(tag + "GUI: control window rendered frames", ctl_.frameCount() >= 10, std::to_string(ctl_.frameCount()) + " frames");
            check(tag + "Overlay: visible and rendering", ovl_.visible() && ovl_.frameCount() >= 10, std::to_string(ovl_.frameCount()) + " frames");
            check(tag + "Overlay: click-through by default", ovl_.clickThroughStyleActive());

            // Layout edit toggles click-through and back.
            setLayoutEdit(true);
            const bool interactiveOk = !ovl_.clickThroughStyleActive();
            setLayoutEdit(false);
            check(tag + "Overlay: layout-edit mode switches click-through off and on", interactiveOk && ovl_.clickThroughStyleActive());

            // Resize handling: swap chain follows the client area.
            SetWindowPos(ovl_.hwnd(), HWND_TOPMOST, 0, 0, 800, 600, SWP_NOACTIVATE);
            pumpMessages();
            ovl_.renderFrame([] {});
            int bw = 0, bh = 0;
            ovl_.queryBackBufferSize(bw, bh);
            RECT cr{};
            GetClientRect(ovl_.hwnd(), &cr);
            check(tag + "Overlay: back buffer follows window resize", bw == cr.right - cr.left && bh == cr.bottom - cr.top && bw == 800,
                  std::to_string(bw) + "x" + std::to_string(bh));

            // Show / hide.
            ctl_.hideWindow();
            const bool hidden = !ctl_.shown();
            ctl_.showWindow();
            check(tag + "GUI: hide and show window", hidden && ctl_.shown());

            // Stop -> idle, then live mode with no game.
            worker_->stop();
            Sleep(400);
            check(tag + "Start/Stop: stop returns to idle", worker_->status().link == Link::Idle || worker_->status().link == Link::GameDetected,
                  toString(worker_->status().link));

            settings_.demoMode = false;
            syncWorker();
            worker_->start();
            Sleep(1500);
            const SessionStatus live = worker_->status();
            check(tag + "Game detection: absent process handled (no crash, waiting state)", live.link == Link::Waiting && !live.game.found,
                  std::string(toString(live.link)) + ": " + live.detail);
            check(tag + "Game detection: no snapshot is produced without a game", !worker_->snapshot().valid);
            worker_->stop();
            settings_.demoMode = true;
            syncWorker();

            destroyWindows();
            check(tag + "GUI: windows destroyed cleanly", !ctl_.isOpen() && !ovl_.isOpen());
            afterCycle[cycle] = measureResources();
        }

        check("Resources: no GDI/USER object growth between cycles",
              afterCycle[1].gdi <= afterCycle[0].gdi && afterCycle[1].user <= afterCycle[0].user,
              "gdi " + std::to_string(base.gdi) + "->" + std::to_string(afterCycle[0].gdi) + "->" + std::to_string(afterCycle[1].gdi) +
              ", user " + std::to_string(base.user) + "->" + std::to_string(afterCycle[0].user) + "->" + std::to_string(afterCycle[1].user));
        check("Resources: no handle growth between cycles", afterCycle[1].handles <= afterCycle[0].handles + 2,
              "handles " + std::to_string(base.handles) + "->" + std::to_string(afterCycle[0].handles) + "->" + std::to_string(afterCycle[1].handles));
    }

    // Settings persistence through the real save path.
    {
        settings_.fovRadius = 345.f;
        settings_.hotkeys.startStop = *keys::vkFromName("F10");
        check("Settings: save to disk", saveSettings());
        Settings back;
        const auto lr = config::load(back, settingsPath_);
        check("Settings: reload equals saved", lr.fileFound && back.fovRadius == 345.f && back.hotkeys.startStop == settings_.hotkeys.startStop);
    }

    // Logs
    {
        logInfo("selftest log marker");
        Logger::global().closeFile();
        std::ifstream in(logPath_);
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        check("Logging: file sink contains runtime entries", all.find("selftest log marker") != std::string::npos &&
              all.find("link:") != std::string::npos);
    }

    // Clean shutdown time
    {
        const auto t0 = Clock::now();
        worker_->start();
        Sleep(100);
        shutdown();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
        check("Shutdown: worker joins promptly, no hang", ms < 2000, std::to_string(ms) + " ms");
    }

finish:
    int failed = 0;
    std::string report = "# RobRun Overlay self test\n\n| Check | Result | Detail |\n|---|---|---|\n";
    for (const auto& r : results) {
        if (!r.ok) ++failed;
        report += "| " + r.name + " | " + (r.ok ? "PASS" : "**FAIL**") + " | " + r.detail + " |\n";
    }
    report += "\n" + std::to_string(results.size()) + " checks, " + std::to_string(failed) + " failed\n";
    fs::path reportPath = opts_.reportPath.empty() ? fs::path("selftest_report.txt") : opts_.reportPath;
    { std::ofstream f(reportPath, std::ios::binary | std::ios::trunc); f << report; }
    writeStdout("\n" + std::to_string(results.size()) + " checks, " + std::to_string(failed) + " failed\n");
    fs::remove_all(tmp, ec);
    return failed == 0 && !results.empty() ? 0 : 1;
}
