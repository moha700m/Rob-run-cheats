// EspRenderer.h - Draws ESP and the HUD on the overlay's ImGui draw lists.
//
// ESP is only drawn for a valid snapshot (live or demo). Demo data is always
// accompanied by an unmissable "DEMO DATA" banner that cannot be switched off.
#pragma once
#include "config/Settings.h"
#include "core/DataWorker.h"
#include "game/GameData.h"

namespace esp {

    void renderEsp(const WorldSnap& world, const Settings& cfg, float screenW, float screenH);

    // Banner + HUD panel. When `layoutEdit` is true the panel can be dragged and
    // resized with the mouse and its rectangle is written back into `cfg`.
    void renderHud(const SessionStatus& status, Settings& cfg, bool layoutEdit,
                   float fps, float screenW, float screenH);

} // namespace esp
