// ControlPanel.h - ImGui content of the main (control) window: Dashboard, Logs,
// Settings and About. Pure UI: it edits the Settings it is given and reports
// user intent through PanelActions; the Application performs the actions.
#pragma once
#include "config/Settings.h"
#include "core/DataWorker.h"
#include "game/SdkOffsets.h"
#include "logging/Logger.h"

#include <string>
#include <vector>

struct PanelModel {
    Settings*      settings = nullptr;
    SessionStatus  status;
    Logger*        logger = nullptr;

    // Offsets / config state
    std::vector<std::string> offsetsMissing;
    std::vector<std::string> offsetsWarnings;
    std::string offsetsPath;
    bool        offsetsFileFound = false;
    std::string buildLabel;
    std::string settingsPath;
    bool        settingsDirty = false;

    // Runtime state
    bool        overlayShown  = false;
    bool        layoutEditing = false;
    int         capturingSlot = -1;     // hotkey being rebound, -1 = none
    std::string renderer;               // "hardware" / "WARP (software)"
    float       fps = 0.f;
    std::string statusLine;             // transient message (saved, reloaded, ...)
};

struct PanelActions {
    bool startStop       = false;
    bool saveSettings    = false;
    bool reloadSettings  = false;
    bool resetDefaults   = false;
    bool reloadOffsets   = false;
    bool writeOffsetsTemplate = false;
    bool toggleLayoutEdit = false;
    bool toggleOverlay   = false;
    bool hideWindow      = false;
    bool quit            = false;
    bool openLogFile     = false;
    bool openFolder      = false;
    int  rebindSlot      = -1;
};

struct PanelUiState {
    std::vector<LogEntry> logCache;
    uint64_t lastLogId = 0;
    int  minLevel = 0;
    char filter[96] = {};
    bool autoScroll = true;
};

PanelActions drawControlPanel(PanelModel& m, PanelUiState& ui, float width, float height);

// Dark theme used by the control window.
void applyControlTheme();
