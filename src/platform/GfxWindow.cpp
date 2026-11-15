#include "GfxWindow.h"

#include <format>

#include "logging/Logger.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

GfxWindow::~GfxWindow() { destroy(); }

LRESULT CALLBACK GfxWindow::thunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    GfxWindow* self = nullptr;
    if (m == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = static_cast<GfxWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = h;
    } else {
        self = reinterpret_cast<GfxWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self) return self->wndProc(h, m, w, l);
    return DefWindowProcW(h, m, w, l);
}

LRESULT GfxWindow::wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ctx_) {
        ImGui::SetCurrentContext(ctx_);
        if (imguiBackendsUp_ && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    }
    LRESULT r = 0;
    if (onMessage(m, w, l, r)) return r;

    switch (m) {
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) {
            pendingW_ = static_cast<int>(LOWORD(l));
            pendingH_ = static_cast<int>(HIWORD(l));
            needsResize_ = true;
        }
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        break;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

bool GfxWindow::create(const Params& p, std::string& error) {
    destroy();
    params_    = p;
    className_ = p.className;
    inst_      = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &GfxWindow::thunk;
    wc.hInstance     = inst_;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = className_.c_str();
    if (!RegisterClassExW(&wc)) {
        error = "RegisterClassEx failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    classRegistered_ = true;

    HWND h = CreateWindowExW(p.exStyle, className_.c_str(), p.title.c_str(), p.style,
                             p.x, p.y, p.w, p.h, nullptr, nullptr, inst_, this);
    if (!h) {
        error = "CreateWindowEx failed (error " + std::to_string(GetLastError()) + ")";
        destroy();
        return false;
    }
    hwnd_ = h;

    if (!initDevice(error)) { destroy(); return false; }

    IMGUI_CHECKVERSION();
    ctx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    if (p.onContextCreated) p.onContextCreated();

    if (!ImGui_ImplWin32_Init(hwnd_)) { error = "ImGui_ImplWin32_Init failed"; destroy(); return false; }
    if (!ImGui_ImplDX11_Init(device_.Get(), deviceCtx_.Get())) {
        ImGui_ImplWin32_Shutdown();
        error = "ImGui_ImplDX11_Init failed";
        destroy();
        return false;
    }
    imguiBackendsUp_ = true;
    return true;
}

void GfxWindow::destroy() {
    if (ctx_) {
        ImGui::SetCurrentContext(ctx_);
        if (imguiBackendsUp_) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            imguiBackendsUp_ = false;
        }
        ImGui::DestroyContext(ctx_);
        ctx_ = nullptr;
    }
    teardownDevice();
    if (hwnd_) {
        // Detach first so wndProc cannot reach a half-destroyed object.
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_) {
        UnregisterClassW(className_.c_str(), inst_);
        classRegistered_ = false;
    }
    bbW_ = bbH_ = 0;
    needsResize_ = false;
}

bool GfxWindow::initDevice(std::string& error) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd_;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, _countof(levels), D3D11_SDK_VERSION, &sd,
                                               swap_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(),
                                               &got, deviceCtx_.ReleaseAndGetAddressOf());
    driver_ = "hardware";
    if (FAILED(hr)) {
        logWarn("hardware D3D11 device failed (0x{:08X}); falling back to WARP software renderer",
                static_cast<unsigned>(hr));
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           levels, _countof(levels), D3D11_SDK_VERSION, &sd,
                                           swap_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(),
                                           &got, deviceCtx_.ReleaseAndGetAddressOf());
        driver_ = "WARP (software)";
    }
    if (FAILED(hr)) {
        error = std::format("D3D11CreateDeviceAndSwapChain failed (HRESULT 0x{:08X})", static_cast<unsigned>(hr));
        return false;
    }
    if (!createRenderTarget()) { error = "cannot create render target view"; return false; }
    return true;
}

void GfxWindow::teardownDevice() {
    releaseRenderTarget();
    if (deviceCtx_) deviceCtx_->ClearState();
    swap_.Reset();
    deviceCtx_.Reset();
    device_.Reset();
}

bool GfxWindow::createRenderTarget() {
    if (!swap_ || !device_) return false;
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf())))) return false;
    if (FAILED(device_->CreateRenderTargetView(back.Get(), nullptr, rtv_.ReleaseAndGetAddressOf()))) return false;
    D3D11_TEXTURE2D_DESC td{};
    back->GetDesc(&td);
    bbW_ = static_cast<int>(td.Width);
    bbH_ = static_cast<int>(td.Height);
    return true;
}

void GfxWindow::releaseRenderTarget() {
    if (deviceCtx_) deviceCtx_->OMSetRenderTargets(0, nullptr, nullptr);
    rtv_.Reset();
}

void GfxWindow::applyPendingResize() {
    if (!needsResize_ || !swap_) return;
    needsResize_ = false;
    if (pendingW_ <= 0 || pendingH_ <= 0) return;
    releaseRenderTarget();
    const HRESULT hr = swap_->ResizeBuffers(0, static_cast<UINT>(pendingW_), static_cast<UINT>(pendingH_),
                                            DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        logWarn("ResizeBuffers failed (0x{:08X})", static_cast<unsigned>(hr));
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) recoverDevice();
        return;
    }
    createRenderTarget();
}

bool GfxWindow::recoverDevice() {
    logWarn("D3D device lost - recreating (window {})", reinterpret_cast<uintptr_t>(hwnd_));
    ImGui::SetCurrentContext(ctx_);
    if (imguiBackendsUp_) ImGui_ImplDX11_Shutdown();
    teardownDevice();
    std::string err;
    if (!initDevice(err) || !ImGui_ImplDX11_Init(device_.Get(), deviceCtx_.Get())) {
        logError("device recovery failed: {}", err);
        imguiBackendsUp_ = false;   // no further rendering
        return false;
    }
    imguiBackendsUp_ = true;
    ++recoveries_;
    return true;
}

bool GfxWindow::queryBackBufferSize(int& w, int& h) const {
    if (!swap_) return false;
    DXGI_SWAP_CHAIN_DESC d{};
    if (FAILED(swap_->GetDesc(&d))) return false;
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf())))) return false;
    D3D11_TEXTURE2D_DESC td{};
    back->GetDesc(&td);
    w = static_cast<int>(td.Width);
    h = static_cast<int>(td.Height);
    return true;
}

bool GfxWindow::renderFrame(const std::function<void()>& draw) {
    if (!hwnd_ || !device_ || !imguiBackendsUp_) return false;
    if (IsIconic(hwnd_) || !IsWindowVisible(hwnd_)) return false;

    applyPendingResize();
    if (!rtv_) return false;

    ImGui::SetCurrentContext(ctx_);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (draw) draw();
    ImGui::Render();

    ID3D11RenderTargetView* rtv = rtv_.Get();
    deviceCtx_->OMSetRenderTargets(1, &rtv, nullptr);
    deviceCtx_->ClearRenderTargetView(rtv, params_.clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const HRESULT hr = swap_->Present(params_.vsync ? 1 : 0, 0);
    ++frames_;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        HRESULT reason = device_ ? device_->GetDeviceRemovedReason() : hr;
        logWarn("Present reported device loss (0x{:08X}, reason 0x{:08X})", static_cast<unsigned>(hr), static_cast<unsigned>(reason));
        recoverDevice();
        return false;
    }
    return true;
}
