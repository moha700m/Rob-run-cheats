// Application.h - Wires settings, offsets, providers, worker thread, control
// window and overlay together, and owns the main loop.
#pragma once
#include "config/Settings.h"
#include "core/DataWorker.h"
#include "core/Keys.h"
#include "game/DemoProvider.h"
#include "game/MemoryProvider.h"
#include "game/SdkOffsets.h"
#include "overlay/Overlay.h"
#include "ui/ControlPanel.h"
#include "ui/ControlWindow.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct CliOptions {
    bool selfTest  = false;
    bool demo      = false;
    bool autoStart = false;
    bool help      = false;
    std::filesystem::path configPath;
    std::filesystem::path logPath;
    std::filesystem::path reportPath;   // selftest report
};

// Returns false and fills `error` on a malformed command line.
bool parseCommandLine(int argc, wchar_t** argv, CliOptions& out, std::string& error);
std::string usageText();

class Application {
public:
    explicit Application(CliOptions opts);
    ~Application();

    int run();           // interactive
    int runSelfTest();   // automated GUI/runtime smoke test (used by CI)

private:
    using Clock = std::chrono::steady_clock;

    bool initCommon(std::string& error);
    void shutdown();
    bool createWindows(std::string& error);
    void destroyWindows();

    bool pumpMessages();                 // false once WM_QUIT is seen
    void frame();                        // one iteration of the main loop body
    void pollHotkeys();
    void syncWorker();
    void updateOverlay(const SessionStatus& st);
    void renderControl(const SessionStatus& st);
    void renderOverlay(const SessionStatus& st);
    void handleActions(const PanelActions& a);

    void toggleRun();
    void setLayoutEdit(bool on);
    void setStatus(const std::string& s);
    bool saveSettings();
    bool reloadSettings();
    void reloadOffsets();
    void applyProviderConfig();
    void refreshDirty();

    static std::filesystem::path exeDir();
    static bool dirWritable(const std::filesystem::path& dir);

    CliOptions opts_;
    std::filesystem::path exeDir_, settingsPath_, offsetsPath_, logPath_;

    Settings settings_;
    std::string savedText_;
    bool dirty_ = false;
    Clock::time_point lastDirtyCheck_{};

    SdkOffsets offsets_;
    bool offsetsFound_ = false;
    std::vector<std::string> offsetsWarnings_;

    MemoryProvider live_;
    DemoProvider   demo_;
    std::unique_ptr<DataWorker> worker_;

    ControlWindow ctl_;
    Overlay       ovl_;
    PanelUiState  panelUi_;
    keys::EdgeTracker keys_;

    int  capturingSlot_ = -1;
    bool layoutEdit_    = false;
    bool quitRequested_ = false;
    bool commonReady_   = false;

    HWND gameHwnd_ = nullptr;
    DWORD gamePid_ = 0;
    Clock::time_point lastPlace_{};
    Clock::time_point lastWindowSearch_{};

    std::string statusLine_;
    Clock::time_point statusSetAt_{};
    std::wstring lastProcessName_;
};
