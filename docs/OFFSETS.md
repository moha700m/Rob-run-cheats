# Offsets (`offsets.ini`)

This project ships **no** real RobRun values. The old compile-time `SDK.h` contained only `0x0` placeholders; it was replaced by a runtime file so values can change without rebuilding and can be validated against the running build. Unset ≠ zero: a missing line is "unset"; a real zero must be written (`entity.team = 0x0`).

Create the template from the app (**Dashboard → Create offsets.template.ini**) or copy `offsets.template.ini`, rename to `offsets.ini` next to the exe, and fill in values from **your own dump** of the exact `RobRun.exe` build you run.

## Required values

| Key | Meaning |
|---|---|
| `rva.view_matrix` | 4×4 float matrix (row-major, view×projection). Offset from the `RobRun.exe` image base. |
| `rva.local_player` | Address (RVA) that *holds a pointer* to the local player struct. |
| `rva.entity_list` | Address (RVA) that *holds a pointer* to an array of entity pointers. |
| `entity.position` | 3 × float (x, y, z), Z-up, inside each entity struct. |
| `entity.health` | float (or int32 with `layout.health_is_int32 = true`). |
| `entity.team` | int32. |

Optional: `rva.entity_count`, `rva.game_state` (non-zero = in match), `entity.max_health`, `entity.is_alive`, `entity.name`, `entity.head_position`, `entity.visible`, `entity.is_local_flag`, and layout options (`layout.entity_stride`, `layout.max_entities`, `layout.name_is_pointer`, …).

## Build identity (strongly recommended)

`build.module_size` = `SizeOfImage` of the module for the build the values came from. If set, the app refuses to read when the running module has a different size and logs why. `build.label` is free text shown in the UI.

## How to find the values (your own dump)

1. **View matrix** — scan for a float matrix that changes when the camera moves (Cheat Engine), confirm in ReClass.NET that the 16 floats look like a projection; or xref the renderer's constant-buffer upload in IDA/Ghidra.
2. **Local player / entity list** — find the health value by scanning, use *Find what accesses this address*, walk back to the struct base, then run a pointer scan for the base. The static pointer's offset from the module base is the RVA.
3. **Entity fields** — in ReClass.NET set the entity base and identify position/health/team by controlled experiments (move, take damage, switch team).
4. Prefer a signature + RIP-relative resolve (`src/platform/MemoryScan.h`, `core/Signature.h`) over raw RVAs if you want offsets to survive patches.

## What the app validates before using anything

Pointers must be user-mode canonical and non-null; matrix finite and non-zero; positions finite and bounded; health finite and in range; names printable ASCII; entity count bounded (≤ 4096). Reads that fail validation skip that entity, or fail the whole snapshot for required values (reported in the UI/log, never silently replaced by guesses).
