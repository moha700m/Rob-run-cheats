#include "EspRenderer.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace {

    ImU32 packColor(const float c[4]) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
    }

    void outlineText(ImDrawList* dl, const ImVec2& at, ImU32 fg, ImU32 shadow, const char* text) {
        dl->AddText({ at.x - 1, at.y }, shadow, text);
        dl->AddText({ at.x + 1, at.y }, shadow, text);
        dl->AddText({ at.x, at.y - 1 }, shadow, text);
        dl->AddText({ at.x, at.y + 1 }, shadow, text);
        dl->AddText(at, fg, text);
    }

    void drawBox(ImDrawList* dl, const Vec2& tl, const Vec2& br, ImU32 color, int thickness) {
        dl->AddRect({ tl.x - 1, tl.y - 1 }, { br.x + 1, br.y + 1 }, IM_COL32(0, 0, 0, 160), 0.0f, 0, 1.0f);
        dl->AddRect({ tl.x, tl.y }, { br.x, br.y }, color, 0.0f, 0, static_cast<float>(thickness));
    }

    void drawHealthBar(ImDrawList* dl, const Vec2& tl, const Vec2& br, float ratio) {
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        const float barW = 3.0f;
        const float x1 = tl.x - barW - 2.0f;
        const float h = (br.y - tl.y) * ratio;
        dl->AddRectFilled({ x1 - 1, tl.y - 1 }, { x1 + barW + 1, br.y + 1 }, IM_COL32(0, 0, 0, 200));
        const int r = static_cast<int>((1.0f - ratio) * 255);
        const int g = static_cast<int>(ratio * 255);
        dl->AddRectFilled({ x1, br.y - h }, { x1 + barW, br.y }, IM_COL32(r, g, 0, 255));
    }

} // namespace

namespace esp {

void renderEsp(const WorldSnap& world, const Settings& cfg, float screenW, float screenH) {
    if (!cfg.espEnabled) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    if (cfg.drawCrosshair) {
        const ImU32 c = packColor(cfg.colCrosshair);
        const float cx = screenW * 0.5f, cy = screenH * 0.5f;
        dl->AddLine({ cx - 6, cy }, { cx + 6, cy }, c, 1.0f);
        dl->AddLine({ cx, cy - 6 }, { cx, cy + 6 }, c, 1.0f);
    }
    if (cfg.drawFovCircle) {
        dl->AddCircle({ screenW * 0.5f, screenH * 0.5f }, cfg.fovRadius, packColor(cfg.colCrosshair), 64, 1.0f);
    }

    if (!world.valid || !world.inMatch || world.source == DataSource::None) return;

    const ImU32 colEnemy = packColor(cfg.colEnemy), colTeam = packColor(cfg.colTeam);
    const ImU32 colText = packColor(cfg.colText), colShadow = packColor(cfg.colShadow);

    for (const auto& e : world.entities) {
        if (!e.alive || e.isLocal) continue;

        const bool friendly = (e.team == world.localTeam);
        if (cfg.espTeamCheck && friendly) continue;

        const float dist = world.localPos.distanceTo(e.pos);
        if (dist > cfg.maxDistance || dist < cfg.minDistance) continue;

        Vec2 tl, br;
        if (!worldBoxToScreen(e.pos, 1.8f, 0.6f, world.viewMatrix, screenW, screenH, tl, br)) continue;
        if (br.x < 0 || tl.x > screenW || br.y < 0 || tl.y > screenH) continue;

        const ImU32 col = friendly ? colTeam : colEnemy;
        if (cfg.espBox) drawBox(dl, tl, br, col, cfg.boxThickness);
        if (cfg.espHealthBar && e.maxHp > 0.0f) drawHealthBar(dl, tl, br, e.hp / e.maxHp);
        if (cfg.espSnapline) dl->AddLine({ screenW * 0.5f, screenH }, { (tl.x + br.x) * 0.5f, br.y }, col, 1.0f);
        if (cfg.espName && !e.name.empty()) outlineText(dl, { tl.x, tl.y - 14 }, colText, colShadow, e.name.c_str());
        if (cfg.espDistance) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "[%dm]", static_cast<int>(dist));
            outlineText(dl, { tl.x, br.y + 2 }, colText, colShadow, buf);
        }
    }
}

void renderHud(const SessionStatus& st, Settings& cfg, bool layoutEdit, float fps, float screenW, float screenH) {
    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    // Unmissable, non-configurable marker whenever the data is synthetic.
    if (st.link == Link::Demo) {
        const char* msg = "DEMO MODE - SYNTHETIC DATA, NOT FROM THE GAME";
        const ImVec2 sz = ImGui::CalcTextSize(msg);
        const float x = (screenW - sz.x) * 0.5f;
        bg->AddRectFilled({ x - 10, 6 }, { x + sz.x + 10, 12 + sz.y }, IM_COL32(200, 110, 0, 220), 4.0f);
        bg->AddText({ x, 9 }, IM_COL32(255, 255, 255, 255), msg);
    }

    if (!cfg.showHud && !layoutEdit) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (!layoutEdit) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowPos({ cfg.hudX, cfg.hudY }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ cfg.hudW, cfg.hudH }, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos({ cfg.hudX, cfg.hudY }, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({ cfg.hudW, cfg.hudH }, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 120, 60 }, { screenW, screenH });
    }
    ImGui::SetNextWindowBgAlpha(cfg.hudOpacity);

    if (ImGui::Begin(layoutEdit ? "RobRun HUD (layout edit)###hud" : "RobRun HUD###hud", nullptr, flags)) {
        ImGui::TextUnformatted("RobRun Overlay");
        ImGui::Separator();

        ImVec4 col(0.7f, 0.7f, 0.7f, 1.f);
        switch (st.link) {
        case Link::Live:  col = { 0.3f, 0.9f, 0.5f, 1.f }; break;
        case Link::Demo:  col = { 1.0f, 0.65f, 0.1f, 1.f }; break;
        case Link::Error: case Link::SdkNotConfigured: col = { 1.0f, 0.4f, 0.4f, 1.f }; break;
        default: break;
        }
        ImGui::TextColored(col, "%s", toString(st.link));
        ImGui::TextWrapped("%s", st.detail.c_str());
        ImGui::Text("Entities: %u   Updates/s: %.0f   FPS: %.0f", st.entityCount, st.updatesPerSec, fps);
        if (layoutEdit) {
            ImGui::Separator();
            ImGui::TextColored({ 1.f, 0.9f, 0.3f, 1.f }, "Drag the title bar to move, drag the corner to resize.");
        }

        if (layoutEdit) {
            const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
            cfg.hudX = p.x; cfg.hudY = p.y; cfg.hudW = s.x; cfg.hudH = s.y;
        }
    }
    ImGui::End();
}

} // namespace esp
