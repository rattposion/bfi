#include "features.h"

namespace ESP {
    void Draw() {
        if (!C.esp) return;

        ImGuiIO& io = ImGui::GetIO();
        float sh = io.DisplaySize.y;
        auto* dl = ImGui::GetBackgroundDrawList();

        ImVec4 col(C.espColor[0], C.espColor[1], C.espColor[2], C.espColor[3]);
        ImU32  ucol = ImGui::GetColorU32(col);

        SDK::dbg_drawn = 0;

        // ZERO il2cpp aqui: usa scrHead/scrFeet projetados na main thread
        EnterCriticalSection(&g_targetCs);
        int n = g_targetCount;
        for (int i = 0; i < n; i++) {
            Target& t = g_targets[i];
            if (!t.player || t.player == g_localPlayer) continue;
            if (C.teamCheck && g_localTeam >= 0 && t.team == g_localTeam) continue;
            if (!t.alive || !t.onScreen) continue;

            float hx = t.scrHead.x, hy = sh - t.scrHead.y + C.espOffY;
            float fx = t.scrFeet.x, fy = sh - t.scrFeet.y + C.espOffY;
            float height = (fy - hy) * C.espScaleY;
            if (height < 2) continue;
            float width = height * 0.42f;
            SDK::dbg_drawn++;

            if (C.espBox)
                dl->AddRect(ImVec2(hx - width / 2, hy), ImVec2(hx + width / 2, fy), ucol, 0, 0, 1.5f);

            if (C.espSnapline)
                dl->AddLine(ImVec2(io.DisplaySize.x / 2, sh), ImVec2(hx, fy), ucol, 1.f);

            if (C.espHealth) {
                const float barW = 5.f;
                float bx = hx - width / 2 - 9.f;
                float by = hy;
                float bh = height;
                float pct = t.hp / 100.f;
                if (pct < 0.f) pct = 0.f;
                if (pct > 1.f) pct = 1.f;

                dl->AddRectFilled(ImVec2(bx - 1, by - 1), ImVec2(bx + barW + 1, by + bh + 1),
                    IM_COL32(0, 0, 0, 230), 2.f);
                dl->AddRect(ImVec2(bx - 1, by - 1), ImVec2(bx + barW + 1, by + bh + 1),
                    IM_COL32(255, 255, 255, 170), 2.f);

                float fh = bh * pct;
                float r = 1.00f - 0.75f * pct;
                float g = 0.15f + 0.85f * pct;
                ImU32 hcol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.15f, 1.f));
                dl->AddRectFilled(ImVec2(bx, by + bh - fh), ImVec2(bx + barW, by + bh), hcol, 1.f);

                if (C.espHealthNum) {
                    char b[16];
                    sprintf_s(b, "%d", t.health);
                    ImVec2 ts = ImGui::CalcTextSize(b);
                    dl->AddText(ImVec2(bx - ts.x - 4.f, by - 2.f), IM_COL32(255, 255, 255, 235), b);
                }
            }

            if (C.espName && !t.name.empty() && t.name != "?") {
                ImVec2 ts = ImGui::CalcTextSize(t.name.c_str());
                dl->AddText(ImVec2(hx - ts.x / 2 + 1, hy - 15), IM_COL32(0, 0, 0, 220), t.name.c_str());
                dl->AddText(ImVec2(hx - ts.x / 2, hy - 16), ucol, t.name.c_str());
            }

            if (C.espWeapon && !t.weapon.empty()) {
                ImVec2 ts = ImGui::CalcTextSize(t.weapon.c_str());
                dl->AddText(ImVec2(hx - ts.x / 2, fy + 15), IM_COL32(200, 200, 255, 220), t.weapon.c_str());
            }

            if (C.espDistance) {
                char b[32];
                sprintf_s(b, "%dm", (int)t.dist);
                ImVec2 ts = ImGui::CalcTextSize(b);
                dl->AddText(ImVec2(hx - ts.x / 2, fy + 3), IM_COL32(255, 255, 255, 230), b);
            }
        }
        LeaveCriticalSection(&g_targetCs);

        // ---- circulo de FOV do aimbot ----
        if (C.aimbot && C.showFovCircle) {
            float cx = io.DisplaySize.x * 0.5f;
            float cy = sh * 0.5f;
            ImU32 fc = C.aimKeyDown
                ? IM_COL32(0, 255, 120, 220)
                : IM_COL32(0, 200, 255, 130);
            dl->AddCircle(ImVec2(cx, cy), C.aimFov, fc, 96, 1.5f);
        }

        if (C.crosshair) {
            float cx = io.DisplaySize.x / 2;
            float cy = sh / 2;
            dl->AddLine(ImVec2(cx - 8, cy), ImVec2(cx + 8, cy), IM_COL32(255, 255, 255, 180));
            dl->AddLine(ImVec2(cx, cy - 8), ImVec2(cx, cy + 8), IM_COL32(255, 255, 255, 180));
        }
    }
}