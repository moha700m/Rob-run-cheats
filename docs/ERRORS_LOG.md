# Errors and verified fixes

Only entries whose fix was confirmed (by a test or a CI run) are listed.

| # | Symptom | Root cause | Fix | Confirmed by |
|---|---|---|---|---|
| 1 | CI failures on early commits: `Math.h` / include errors on Windows | A project header named `Math.h` collides with `<math.h>` on case-insensitive NTFS | Renamed to `VecMath.h` (commit history: "remove case-conflicting Math.h filename") | CI run on `main` after that commit: success |
| 2 | Unit test `settings.duplicate_hotkeys_are_repaired` failed | Repair reset the colliding slot to its default, which could equal the other slot's key (`F6`) | Reassign to the slot's default, else the first unused F-key; loop until no collisions | `settings.*` incl. `hotkey_repair_never_leaves_collisions` |
| 3 | MinGW cross-build: `undefined reference to MemoryProvider::…` in tests | `MemoryProvider.cpp` was only linked into the app | Separate `robrun_memory` library linked by app and tests | MinGW build + 9/9 `process.*` tests under Wine |
| 4 | MinGW link: `undefined reference to D3DCompile` | ImGui DX11 backend needs `d3dcompiler` (MSVC links it via pragma) | Link `d3dcompiler` explicitly | MinGW build |
| 5 | Warning: `WIN32_LEAN_AND_MEAN` redefined | Header defined it unconditionally | `#ifndef` guards | MinGW build without warning |
| 6 | Original `SDK.h`: unset offsets were `0x0`, indistinguishable from a real zero | Design | `SdkOffsets` uses `std::optional`; zero must be written explicitly | `offsets.zero_is_a_real_value_distinct_from_unset` |
