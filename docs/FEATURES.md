# Features — completed vs incomplete

"Evidence" refers to tests in `tests/` or the CI self test. **Nothing here has been verified against the real RobRun.exe** — no build of it was available and no offsets are shipped.

## Complete (implemented and covered)

| Feature | Evidence |
|---|---|
| Main window, Dashboard/Logs/Settings/About tabs | self test: windows created, frames rendered, hide/show |
| Start/Stop, link state, status text | `worker.*` tests; self test start/stop cycles |
| Detect `RobRun.exe` (configurable name), handle absence | `process.absent_*`, `worker.*`, self test (absent-process wait state) |
| Live log view + file sink + rotation | `logger.*`; self test (file contains runtime entries) |
| Settings save/load, validation, atomic write | `settings.*`; self test |
| Rebindable global hotkeys, conflict repair | `keys.*`, `settings.hotkey_*` |
| Transparent click-through topmost overlay, no focus steal | self test (click-through style), by construction (`NOACTIVATE`) |
| Move/resize overlay HUD (layout-edit mode) | self test (style toggles); drag/resize interaction itself needs manual check |
| Display modes: fit game window / full-screen monitor | by construction; **manual check needed** |
| Resize / device-loss handling, WARP fallback | self test (back buffer follows resize); device-loss path not exercised |
| Read-only memory path with validation, build identity check | `process.*` end-to-end against an in-process fixture |
| Demo Mode (always labelled) | `demo.*`, self test |
| Clean shutdown, no resource growth across create/destroy cycles | self test (GDI/USER/handle counts), `worker.shutdown_*` |
| CI: one build per commit, x64 PE check, tests, artifact | `.github/workflows/build.yml` |

## Incomplete / not possible without user data

| Item | Status |
|---|---|
| **Real game data / real ESP** | **Not verified.** Needs `offsets.ini` from your own RobRun dump. Code path is tested only against a fixture. |
| Skeleton ESP | Not implemented (needs bone data). Setting is shown disabled. |
| Team/visibility semantics | `team`, `visible`, `is_alive` interpretations are assumptions until checked on the real game. |
| Arabic UI text | UI is English; ImGui's default pipeline has no Arabic shaping/RTL. |
| Exclusive-fullscreen games | A topmost window cannot draw over exclusive fullscreen; borderless/windowed only. |
| Multi-monitor / mixed-DPI placement | Uses the game window's monitor; not tested on mixed DPI. |
| Rendering on real GPUs | CI runs on a GPU-less runner (WARP). Hardware rendering is untested here. |
