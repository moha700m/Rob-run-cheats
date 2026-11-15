# Architecture

```
            UI thread (single, owns both windows)                 Worker thread
 ┌───────────────────────────────────────────────────┐      ┌──────────────────────────┐
 │ Application::frame()                              │      │ DataWorker::loop()        │
 │   hotkeys (GetAsyncKeyState edge detect)          │ start/stop/demo/interval (atomics)│
 │   ControlWindow ── ControlPanel (ImGui ctx #1)    │─────▶│ probe game @1 Hz          │
 │   Overlay ─────── EspRenderer  (ImGui ctx #2)     │◀─────│ provider.update() @ interval
 │        ▲ reads status()/snapshot() copies         │ mutex │ publishes SessionStatus + WorldSnap
 └───────────────────────────────────────────────────┘      └──────────┬───────────────┘
                                                                        │ IGameDataProvider
                                                          ┌─────────────┴───────────────┐
                                                   MemoryProvider (Windows)       DemoProvider (portable)
                                                   OpenProcess VM_READ only       synthetic, tagged DataSource::Demo
```

* **Two windows, two D3D11 devices, two ImGui contexts** (`GfxWindow`). The overlay is `WS_EX_LAYERED | TRANSPARENT | TOPMOST | NOACTIVATE`, so the game keeps all input; it only becomes interactive while editing the HUD layout. Resize → swap-chain `ResizeBuffers` on the next frame; `DEVICE_REMOVED/RESET` → device and ImGui DX11 backend are rebuilt; no hardware device → WARP fallback.
* **Only the worker thread touches providers**; the UI reads copies. `DataWorker::shutdown()` requests stop through `std::jthread` + `condition_variable_any`, so quitting never waits out the read interval.
* **Link states** (`Idle, GameDetected, Waiting, SdkNotConfigured, Live, Demo, Error`) are computed from provider results, logged once per transition, and shown verbatim in the UI/HUD. `Live` is only reported when `MemoryProvider::update()` returned validated data.
* **Portable core** (everything except `platform/`, `overlay/`, `ui/`, `app/`, `MemoryProvider`) has no OS headers, so the Linux CI job can unit-test settings, logging, offsets parsing, math, demo data and the worker with fake providers.
* **Single-instance resource ownership:** windows, devices, contexts and handles are RAII (`ComPtr`, `GfxWindow::destroy()`, `Process::~Process`); failed init and normal shutdown share `destroy()`.
