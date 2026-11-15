#include "Overlay.h"

#include "logging/Logger.h"

#include "imgui.h"

#include <dwmapi.h>

#include <vector>

bool Overlay::create(std::string& error) {
    Params p;
    p.className = L"RobRunOverlayWnd_8f12";
    p.title     = L"RobRun Overlay";
    p.style     = WS_POPUP;
    p.exStyle   = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    p.x = 0; p.y = 0; p.w = 640; p.h = 360;
    p.clearColor[0] = p.clearColor[1] = p.clearColor[2] = p.clearColor[3] = 0.0f;   // fully transparent
    p.vsync = false;   // frame pacing is done by the application loop
    p.onContextCreated = [] {
        ImGui::StyleColorsDark();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    };
    if (!GfxWindow::create(p, error)) return false;

    // Per-pixel alpha through DWM. If composition is unavailable, fall back to a
    // colour key (black becomes transparent) so the overlay still works.
    BOOL comp = FALSE;
    DwmIsCompositionEnabled(&comp);
    composition_ = comp != FALSE;
    if (composition_) {
        const MARGINS m{ -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd(), &m);
        SetLayeredWindowAttributes(hwnd(), 0, 255, LWA_ALPHA);
    } else {
        logWarn("DWM composition is disabled; overlay uses colour-key transparency (black = transparent)");
        SetLayeredWindowAttributes(hwnd(), RGB(0, 0, 0), 255, LWA_COLORKEY);
    }
    ShowWindow(hwnd(), SW_HIDE);
    visible_ = false;
    interactive_ = false;
    logInfo("overlay created ({} renderer, transparency: {})", driverName(), composition_ ? "DWM alpha" : "colour key");
    return true;
}

void Overlay::setVisible(bool v) {
    if (!isOpen() || v == visible_) return;
    visible_ = v;
    if (v) {
        ShowWindow(hwnd(), SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        ShowWindow(hwnd(), SW_HIDE);
    }
}

bool Overlay::clickThroughStyleActive() const {
    if (!isOpen()) return false;
    return (GetWindowLongPtrW(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0;
}

void Overlay::setInteractive(bool on) {
    if (!isOpen() || on == interactive_) return;
    interactive_ = on;
    LONG_PTR ex = GetWindowLongPtrW(hwnd(), GWL_EXSTYLE);
    if (on) ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    else    ex |=  static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(hwnd(), GWL_EXSTYLE, ex);
    SetWindowPos(hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool Overlay::place(HWND target, OverlayMode mode) {
    if (!isOpen()) return false;
    RECT r{};
    bool haveRect = false;

    if (mode == OverlayMode::FitGameWindow && target && IsWindow(target) && !IsIconic(target)) {
        RECT cr{};
        if (GetClientRect(target, &cr)) {
            POINT tl{ cr.left, cr.top };
            if (ClientToScreen(target, &tl)) {
                r = { tl.x, tl.y, tl.x + (cr.right - cr.left), tl.y + (cr.bottom - cr.top) };
                haveRect = (r.right - r.left) > 0 && (r.bottom - r.top) > 0;
            }
        }
    }
    if (!haveRect) {
        HMONITOR mon = (target && IsWindow(target)) ? MonitorFromWindow(target, MONITOR_DEFAULTTOPRIMARY)
                                                    : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) r = mi.rcMonitor;
        else r = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }

    if (EqualRect(&r, &lastRect_)) return false;
    lastRect_ = r;
    SetWindowPos(hwnd(), HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE | (visible_ ? SWP_SHOWWINDOW : 0));
    return true;
}

namespace {
    struct FindCtx { DWORD pid; HWND best; long long bestArea; };

    BOOL CALLBACK enumProc(HWND h, LPARAM lp) {
        auto* c = reinterpret_cast<FindCtx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid != c->pid) return TRUE;
        if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER) != nullptr) return TRUE;
        if (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
        RECT r{};
        if (!GetClientRect(h, &r)) return TRUE;
        const long long area = static_cast<long long>(r.right - r.left) * (r.bottom - r.top);
        if (area > c->bestArea) { c->best = h; c->bestArea = area; }
        return TRUE;
    }
}

HWND Overlay::findMainWindow(DWORD pid) {
    if (!pid) return nullptr;
    FindCtx c{ pid, nullptr, 0 };
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&c));
    return c.best;
}
