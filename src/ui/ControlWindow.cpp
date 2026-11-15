#include "ControlWindow.h"

#include "ControlPanel.h"
#include "logging/Logger.h"

#include "imgui.h"

#include <filesystem>

namespace {
    // Loads Segoe UI (shipped with Windows) for a cleaner look; falls back to the
    // built-in font if it is missing.
    void loadFonts() {
        ImGuiIO& io = ImGui::GetIO();
        wchar_t winDir[MAX_PATH]{};
        if (GetWindowsDirectoryW(winDir, MAX_PATH)) {
            std::filesystem::path p = std::filesystem::path(winDir) / "Fonts" / "segoeui.ttf";
            std::error_code ec;
            if (std::filesystem::exists(p, ec)) {
                if (io.Fonts->AddFontFromFileTTF(p.string().c_str(), 17.0f)) return;
            }
        }
        io.Fonts->AddFontDefault();
    }
}

bool ControlWindow::create(int width, int height, std::string& error) {
    Params p;
    p.className = L"RobRunControlWnd_3c71";
    p.title     = L"RobRun Overlay";
    p.style     = WS_OVERLAPPEDWINDOW;
    p.exStyle   = 0;

    RECT desired{ 0, 0, width, height };
    AdjustWindowRect(&desired, p.style, FALSE);
    p.w = desired.right - desired.left;
    p.h = desired.bottom - desired.top;
    p.x = (GetSystemMetrics(SM_CXSCREEN) - p.w) / 2;
    p.y = (GetSystemMetrics(SM_CYSCREEN) - p.h) / 2;
    if (p.x < 0) p.x = 0;
    if (p.y < 0) p.y = 0;

    p.clearColor[0] = 0.059f; p.clearColor[1] = 0.067f; p.clearColor[2] = 0.090f; p.clearColor[3] = 1.f;
    p.vsync = false;   // frame pacing is done by the application loop
    p.onContextCreated = [] { loadFonts(); applyControlTheme(); };

    if (!GfxWindow::create(p, error)) return false;
    ShowWindow(hwnd(), SW_SHOW);
    UpdateWindow(hwnd());
    logInfo("control window created ({} renderer)", driverName());
    return true;
}

bool ControlWindow::onMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    switch (msg) {
    case WM_CLOSE:
        closeRequested_ = true;
        result = 0;
        return true;
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        mm->ptMinTrackSize.x = 760;
        mm->ptMinTrackSize.y = 520;
        result = 0;
        return true;
    }
    default:
        (void)wp;
        return false;
    }
}

void ControlWindow::showWindow() {
    if (!isOpen()) return;
    ShowWindow(hwnd(), IsIconic(hwnd()) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd());
}

void ControlWindow::hideWindow() {
    if (isOpen()) ShowWindow(hwnd(), SW_HIDE);
}

bool ControlWindow::shown() const {
    return isOpen() && IsWindowVisible(hwnd()) && !IsIconic(hwnd());
}
