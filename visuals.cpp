#include "features.h"

namespace Visuals {

    static void Radar() {
        if (!C.radar || !g_localPlayer) return;
        ImGuiIO& io = ImGui::GetIO();
        const float R = 90.f;
        ImVec2 c(io.DisplaySize.x - R - 20.f, R + 20.f);
        auto* dl = ImGui::GetForegroundDrawList();

        dl->AddCircleFilled(c, R, IM_COL32(10, 10, 10, 140), 64);
        dl->AddCircle(c, R, IM_COL32(0, 255, 180, 180), 64, 1.5f);

        EnterCriticalSection(&g_targetCs);
        for (int i = 0; i < g_targetCount; i++) {
            Target& t = g_targets[i];
            if (!t.player || t.player == g_localPlayer || !t.alive) continue;
            if (C.teamCheck && g_localTeam >= 0 && t.team == g_localTeam) continue;

            Vec3 d = t.feet - g_localPos;
            float scale = R / 60.f;                       // 60m de raio
            float px = d.x * scale, pz = d.z * scale;
            float l = sqrtf(px * px + pz * pz);
            if (l > R - 3.f) { px *= (R - 3.f) / l; pz *= (R - 3.f) / l; }

            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(C.espColor[0], C.espColor[1], C.espColor[2], 1.f));
            dl->AddCircleFilled(ImVec2(c.x + px, c.y - pz), 3.f, col, 12);
        }
        LeaveCriticalSection(&g_targetCs);
    }

    static void Hitmarker() {
        if (!C.hitmarker) return;
        ULONGLONG dt = GetTickCount64() - f::lastHitTime.load();
        if (dt > 250) return;

        float a = 1.f - (float)dt / 250.f;
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 c(io.DisplaySize.x / 2, io.DisplaySize.y / 2);
        ImU32 col = IM_COL32(255, 255, 255, (int)(255 * a));
        auto* dl = ImGui::GetForegroundDrawList();

        dl->AddLine(ImVec2(c.x - 10, c.y - 10), ImVec2(c.x - 4, c.y - 4), col, 2.f);
        dl->AddLine(ImVec2(c.x + 10, c.y - 10), ImVec2(c.x + 4, c.y - 4), col, 2.f);
        dl->AddLine(ImVec2(c.x - 10, c.y + 10), ImVec2(c.x - 4, c.y + 4), col, 2.f);
        dl->AddLine(ImVec2(c.x + 10, c.y + 10), ImVec2(c.x + 4, c.y + 4), col, 2.f);
    }

    void Draw() {
        if (C.watermark) {
            ImGuiIO& io = ImGui::GetIO();
            char b[192];
            sprintf_s(b, "Venice BF | %.0f FPS | alvos: %d | aim:%d silent:%d magic:%d god:%d",
                io.Framerate, g_targetCount, C.aimbot, C.silentAim, C.magicBullets, C.godmode);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(12, 12), IM_COL32(0, 255, 180, 230), b);
        }
        Radar();
        Hitmarker();
        Misc::Draw();
    }
}