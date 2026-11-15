# RobRun External Overlay

External, **read-only** overlay and control app for `RobRun.exe` (Windows x64, C++20, Direct3D 11, Dear ImGui).

> **ملخص بالعربية:** تطبيق Windows خارجي يعمل بنافذة تحكم رئيسية + Overlay شفاف فوق اللعبة. لا حقن، لا Hooks، لا كتابة في ذاكرة اللعبة، ولا تجاوز لأي حماية. **لا توجد Offsets حقيقية في المشروع**: بيانات اللعبة الحقيقية تحتاج ملف `offsets.ini` تستخرجه أنت من نسختك، وبدونه يعمل التطبيق في **Demo Mode** فقط (بيانات وهمية معنونة بوضوح). حالة كل ميزة موثّقة في `docs/FEATURES.md`.

## What it is — and is not

| Is | Is not |
|---|---|
| Separate process, `OpenProcess(PROCESS_VM_READ \| PROCESS_QUERY_INFORMATION)` + `ReadProcessMemory` | Injection, hooks, patches, memory writes, handle stealing |
| Click-through transparent overlay that never takes focus | Anti-cheat bypass, process hiding, driver tricks |
| Control window: Start/Stop, status, live log, settings, hotkeys | A bundle of game offsets (none are shipped) |
| Demo Mode with fabricated data, always labelled `DEMO` | Fake data presented as real |

**Real ESP needs your own offsets.** Without a complete `offsets.ini` the live path refuses to read anything and the UI says exactly which values are missing. See [docs/OFFSETS.md](docs/OFFSETS.md).

## Get it

* **Prebuilt:** GitHub → Actions → latest successful *Build* run → artifact `RobRunOverlay-<sha>` (zip with the EXE, template, docs and the CI test report).
* **From source:** Visual Studio 2022 (Desktop development with C++) + CMake ≥ 3.21 + internet (Dear ImGui `v1.91.9` is fetched by CMake):

```bat
cmake -B build -S . -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
build\Release\RobRunOverlay.exe
```

Or just double-click **`Run_RobRun.bat`** (runs a prebuilt exe if present, otherwise builds, tests, then starts it; it refuses to start if the build or tests fail).

## Using it

1. Start `RobRunOverlay.exe` (run as administrator only if the game runs elevated).
2. **Dashboard** shows whether `RobRun.exe` is running, the link state and what data source is active.
3. Press **START** (default `F8`). With no offsets the state is *"Game found – offsets not configured"*; tick **Demo Mode** to see the overlay with synthetic data.
4. **Logs** tab: live log with level/text filter, copy, clear. The same lines go to `RobRunOverlay.log` (rotated at 1 MiB).
5. **Settings** tab: process name, overlay mode, ESP options, colours, performance, hotkey rebinding. Saved to `RobRunOverlay.ini` next to the exe (falls back to `%APPDATA%\RobRunOverlay` if that folder is read-only); auto-saved on exit.

Default hotkeys (global, rebindable in Settings): `INSERT` show/hide window · `F8` start/stop · `F7` overlay on/off · `F6` edit HUD layout · `F9` Demo Mode · `END` quit.

Command line: `--config <file> --log <file> --demo --autostart --selftest --report <file> --help`.

## Layout

```
src/
  app/        main.cpp (entry, CLI), Application (loop, hotkeys, self test)
  core/       DataWorker (thread), Keys, VecMath, Validate, Signature   (portable)
  config/     Settings load/save                                         (portable)
  logging/    Logger: ring buffer + rotating file                        (portable)
  game/       SdkOffsets (offsets.ini), GameData types, DemoProvider (portable), MemoryProvider (Windows)
  platform/   Process (read-only RPM), MemoryScan, GfxWindow (D3D11 + ImGui per window)
  overlay/    Overlay window (transparent, click-through, topmost)
  ui/         ControlWindow, ControlPanel (tabs), EspRenderer (ESP + HUD)
tests/        unit tests (portable + Windows) — see docs/TEST_REPORT.md
docs/         ARCHITECTURE, OFFSETS, FEATURES, ERRORS_LOG, TEST_REPORT
.github/workflows/build.yml   Linux unit tests + Windows x64 build/test/artifact
```

## Honest status

See [docs/FEATURES.md](docs/FEATURES.md) for what is complete, what is incomplete, and which claims are backed by which test. Compilation success is not treated as proof that the app runs; the CI job also executes the unit tests and an automated GUI/runtime self test (`RobRunOverlay.exe --selftest`).

`preview.py` / `Run_Preview_One_Click.bat` are the older Python/Tkinter *mock-up* of the UI. They do not touch the game or the real app.
