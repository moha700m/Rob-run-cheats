// ControlWindow.h - The application's normal top-level window (opaque, owns the
// control panel UI). Closing it requests a clean shutdown instead of tearing
// the process down from inside the message handler.
#pragma once
#include "platform/GfxWindow.h"

class ControlWindow final : public GfxWindow {
public:
    bool create(int width, int height, std::string& error);

    bool closeRequested() const { return closeRequested_; }
    void clearCloseRequest() { closeRequested_ = false; }

    void showWindow();     // restore + bring to front
    void hideWindow();
    bool shown() const;

protected:
    bool onMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) override;

private:
    bool closeRequested_ = false;
};
