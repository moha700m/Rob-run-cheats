# Test report

Evidence below comes from GitHub Actions run **43747776314** (commit `d76528cad1a1f19391b9490723a21571f0995b32`, workflow *Build*, jobs *Windows x64 Release* and *Portable unit tests (Linux)* — both `success`). The raw reports are in that run's artifact `test-reports-<sha>` and inside the release zip as `TEST_REPORT_CI.md`. Statements marked **NOT TESTED** have no evidence.

## Summary table

| Feature | Status | Test evidence |
|---|---|---|
| GUI | **Passed** (on the CI runner) | `app.selftest`: control window + overlay created with D3D11 + ImGui, 63–120 frames rendered per cycle, hide/show works, windows destroyed cleanly; 2 full create/destroy cycles. Not tried on a real user desktop or GPU. |
| Overlay | **Passed** for window mechanics; **NOT TESTED** over a real game | Self test: visible and rendering, click-through style on by default, layout-edit mode toggles it off/on, back buffer follows a resize to 800×600. Foreground/placement logic against a real game window is untested. |
| Game Detection | **Passed** for the *absent* case; **NOT TESTED** for a real `RobRun.exe` | Self test: absent process → "Waiting for game", no snapshot, no crash (both cycles). `process.*` tests: not-found, attach-to-self, module identity, build-mismatch refusal. Detecting the real game is unverified. |
| Demo Mode | **Passed** | `demo.*` unit tests; self test: link `Demo`, 8 entities, snapshot tagged `Demo`; overlay always draws the "DEMO MODE - SYNTHETIC DATA" banner. |
| Build x64 | **Passed** | Configure + MSVC Release build succeeded; `verify_pe.ps1`: Machine `0x8664` (x64), PE32+, GUI subsystem, 1,257,984 bytes, SHA-256 `63668742399CC1131FD9D9ABDA720B406FAE906A5F7890F555D643AE6043455F`. |
| Runtime | **Passed** (self test only) | `RobRunOverlay.exe --selftest` exit 0, 33/33 checks; GDI 9→9, USER 4→4, handles 226→226 between cycle 1 and 2 (no growth); shutdown joined in 111 ms. This is an automated smoke test, not a long-run or soak test. |
| Settings persistence | Passed | `settings.*` (10 tests incl. clamping, NaN rejection, atomic save) + self test save/reload. |
| Logging | Passed | `logger.*` (ring buffer, rotation, concurrency) + self test (file sink contains runtime entries). |
| Memory read path (read-only) | Passed **against an in-process fixture only** | `process.provider_reads_fixture_world_end_to_end`, null/NaN/Inf rejection, unusable-matrix rejection, wrong-build refusal. **Not verified against RobRun.exe**: no offsets exist. |
| Real ESP on RobRun | **NOT VERIFIED / unavailable** | No offsets are shipped; link state stays *"offsets not configured"* until the user supplies `offsets.ini`. |

## Numbers

* Windows: `ctest` 12/12 passed (10 portable suites + `unit.process` + `app.selftest`); unit runner: 70 tests, 0 failed.
* Linux: `ctest` 10/10 passed.
* Locally (before CI): 68 unit tests (portable + `process.*`) passed under Wine, and the portable suites passed natively on Linux; the GUI self test cannot run under Wine here.

## Known limits of this evidence

* The CI machine is a hosted VM, not a gaming PC; hardware rendering, multi-monitor, high-DPI and exclusive-fullscreen behaviour are untested.
* Resource-leak evidence is a delta between two create/destroy cycles (GDI/USER objects, process handles), not a memory profiler run.
* Hotkeys use `GetAsyncKeyState`; the edge-detection logic is unit-tested, actual key delivery is not.
