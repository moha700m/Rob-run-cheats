// Overlay.h - Transparent, click-through, always-on-top DX11 window that is
// positioned over the game's client area (or the whole monitor).
//
// The overlay never takes focus (WS_EX_NOACTIVATE) and is click-through
// (WS_EX_TRANSPARENT) except while the user is editing the HUD layout, so the
// game keeps receiving all mouse and keyboard input.
#pragma once
#include "config/Settings.h"
#include "platform/GfxWindow.h"

class Overlay final : public GfxWindow {
public:
    bool create(std::string& error);

    void setVisible(bool visible);
    bool visible() const { return visible_; }

    // Click-through is ON when interactive == false.
    void setInteractive(bool interactive);
    bool interactive() const { return interactive_; }

    // True if the extended style currently has WS_EX_TRANSPARENT (for tests).
    bool clickThroughStyleActive() const;

    // Positions the overlay. `target` may be null (then the primary monitor is used).
    // Returns true if the window rectangle changed.
    bool place(HWND target, OverlayMode mode);

    // Finds the main top-level window of a process (largest visible, unowned).
    static HWND findMainWindow(DWORD pid);

    bool compositionEnabled() const { return composition_; }

private:
    bool visible_     = false;
    bool interactive_ = false;
    bool composition_ = true;
    RECT lastRect_{ 0, 0, 0, 0 };
};
