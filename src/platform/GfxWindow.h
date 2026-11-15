// GfxWindow.h - A Win32 window with its own Direct3D 11 device, swap chain and
// Dear ImGui context. Both the control window and the overlay are GfxWindows.
//
// Owns every D3D/ImGui resource it creates (ComPtr / RAII), handles resize,
// device loss (DXGI_ERROR_DEVICE_REMOVED/RESET) and falls back to the WARP
// software rasterizer when no hardware device is available.
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <functional>
#include <string>

struct ImGuiContext;

class GfxWindow {
public:
    struct Params {
        std::wstring className;
        std::wstring title;
        DWORD style   = WS_OVERLAPPEDWINDOW;
        DWORD exStyle = 0;
        int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = 800, h = 600;
        float clearColor[4] = { 0.06f, 0.07f, 0.09f, 1.0f };
        bool  vsync = true;
        std::function<void()> onContextCreated;   // runs with the new ImGui context current (fonts/style)
    };

    GfxWindow() = default;
    virtual ~GfxWindow();
    GfxWindow(const GfxWindow&) = delete;
    GfxWindow& operator=(const GfxWindow&) = delete;

    bool create(const Params& p, std::string& error);
    void destroy();   // idempotent; releases every resource

    bool  isOpen()  const { return hwnd_ != nullptr; }
    HWND  hwnd()    const { return hwnd_; }
    ImGuiContext* context() const { return ctx_; }

    // Client-area size in pixels (what the swap chain was last sized to).
    int width()  const { return bbW_; }
    int height() const { return bbH_; }

    // Runs one ImGui frame: NewFrame, draw(), Render, Present. Returns false
    // when nothing was rendered (minimized / hidden / no device).
    bool renderFrame(const std::function<void()>& draw);

    unsigned long long frameCount()      const { return frames_; }
    int                deviceRecoveries() const { return recoveries_; }
    const std::string& driverName()      const { return driver_; }

    // Size of the swap chain's back buffer as reported by DXGI (for tests).
    bool queryBackBufferSize(int& w, int& h) const;

protected:
    // Return true (and set `result`) to consume the message.
    virtual bool onMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
        (void)msg; (void)wp; (void)lp; (void)result; return false;
    }

private:
    static LRESULT CALLBACK thunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);

    bool initDevice(std::string& error);
    void teardownDevice();
    bool createRenderTarget();
    void releaseRenderTarget();
    void applyPendingResize();
    bool recoverDevice();

    HWND         hwnd_ = nullptr;
    HINSTANCE    inst_ = nullptr;
    std::wstring className_;
    bool         classRegistered_ = false;
    ImGuiContext* ctx_ = nullptr;
    bool         imguiBackendsUp_ = false;
    Params       params_;

    Microsoft::WRL::ComPtr<ID3D11Device>           device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    deviceCtx_;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         swap_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;

    int  bbW_ = 0, bbH_ = 0;
    int  pendingW_ = 0, pendingH_ = 0;
    bool needsResize_ = false;
    unsigned long long frames_ = 0;
    int  recoveries_ = 0;
    std::string driver_;
};
