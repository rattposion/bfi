// ============================================================================
//  esp.cpp — desenho do ESP (thread de render, sem chamada il2cpp)
//  + visual check: verde = visivel, vermelho = parede
//  + linha do centro da mira ate o inimigo (toggle C.espVisLine)
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <imgui.h>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace ESP {

    static inline ImU32 Col(const float c[4], float aMul = 1.f) {
        return IM_COL32((int)(c[0] * 255.f), (int)(c[1] * 255.f),
            (int)(c[2] * 255.f), (int)(c[3] * 255.f * aMul));
    }

    void Draw() {
        SDK::dbg_drawn = 0;

        if (!C.esp) return;
        if (!SDK::g_localPlayer) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        ImU32 colBox = Col(C.espBoxColor);
        ImU32 colText = Col(C.espTextColor);
        ImU32 colSkel = Col(C.espSkelColor);
        ImU32 colVis = Col(C.visColor);
        ImU32 colNoVis = Col(C.noVisColor);
        float th = C.espThickness;

        int drawn = 0;
        float cx = (float)SDK::ScreenW() * 0.5f;      // centro da mira
        float cyMid = (float)SDK::ScreenH() * 0.5f;   // centro vertical (mira)
        float cyBot = (float)SDK::ScreenH();          // base da tela (snapline)

        SDK::LockTargets();
        int n = SDK::g_targetCount;

        for (int i = 0; i < n; ++i) {
            Target& t = SDK::g_targets[i];

            if (!t.player) continue;
            if (t.player == SDK::g_localPlayer) continue;
            if (!t.alive) continue;
            if (C.espNpc && t.npc) continue;
            // >>> FIX: sem o guard "!= 0" (time 0 tambem filtra) e unificado com o aim
            if ((C.teamCheck || C.aimTeam) && t.team == SDK::g_localTeam) continue;
            if (!t.onScreen) continue;
            if (C.espMaxDist > 1.f && t.dist > C.espMaxDist) continue;

            float top = t.scrHead.y + C.espOffY;
            float h = (t.scrFeet.y - top) * C.espScaleY;
            if (h < 4.f || h > 2000.f) continue;

            float w = h * 0.5f * (C.espBodyH / 1.8f);
            float x0 = t.scrHead.x - w * 0.5f;
            float y0 = top;
            float x1 = t.scrHead.x + w * 0.5f;
            float y1 = top + h;

            // ---- visual check: marcador verde/vermelho ----
            if (C.espVisCheck) {
                ImU32 vc = t.visible ? colVis : colNoVis;
                if (C.espVisLine)   // <<< linha do centro ate o inimigo (opcional)
                    dl->AddLine(ImVec2(cx, cyMid), ImVec2(t.scrHead.x, t.scrHead.y), vc, th);
                dl->AddCircleFilled(ImVec2(t.scrHead.x, t.scrHead.y), 3.f, vc);
            }

            // ---- caixa: verde/vermelho quando visCheck, senao cor normal ----
            ImU32 boxCol = colBox;
            if (C.espVisCheck) boxCol = t.visible ? colVis : colNoVis;

            if (C.espFill)
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), Col(C.espBoxColor, 0.20f));

            if (C.espBox)
                dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), boxCol, 0.f, 0, th);

            // ---- alvo atual da mira: marcador extra ----
            if (t.player == SDK::g_aimTarget) {
                ImU32 ac = t.visible ? colVis : colNoVis;
                dl->AddRect(ImVec2(x0 - 2.f, y0 - 2.f), ImVec2(x1 + 2.f, y1 + 2.f), ac, 0.f, 0, th + 1.f);
            }

            if (C.espHealth) {
                float hp = t.health * 0.01f;
                if (!(hp >= 0.f && hp <= 1.f)) hp = 1.f;     // lixo -> cheia
                if (hp > 1.f) hp = 1.f;

                float bw = 4.f;
                float bx = x0 - bw - 2.f;
                dl->AddRectFilled(ImVec2(bx, y0), ImVec2(bx + bw, y1), IM_COL32(0, 0, 0, 180));
                dl->AddRectFilled(ImVec2(bx, y1 - h * hp), ImVec2(bx + bw, y1),
                    IM_COL32((int)(255.f * (1.f - hp)), (int)(255.f * hp), 0, 255));

                if (C.espHealthNum) {
                    char hpBuf[16];
                    sprintf_s(hpBuf, sizeof(hpBuf), "%.0f", (double)t.health);
                    dl->AddText(ImVec2(bx - 22.f, y0), colText, hpBuf);
                }
            }

            if (C.espSkeleton) {
                dl->AddLine(ImVec2(t.scrHead.x, y0), ImVec2(t.scrHead.x, y1), colSkel, th);
                float sy = y0 + h * 0.22f;
                dl->AddLine(ImVec2(x0, sy), ImVec2(x1, sy), colSkel, th);
                dl->AddLine(ImVec2(x0, sy), ImVec2(t.scrHead.x, y0 + h * 0.06f), colSkel, th);
                dl->AddLine(ImVec2(x1, sy), ImVec2(t.scrHead.x, y0 + h * 0.06f), colSkel, th);
            }

            char buf[96];

            if (C.espName && t.name[0]) {
                float tw = (float)strlen(t.name) * (C.espFontSize * 0.55f);
                dl->AddText(ImVec2(t.scrHead.x - tw * 0.5f, y0 - C.espFontSize - 2.f), colText, t.name);
            }

            if (C.espWeapon) {
                sprintf_s(buf, sizeof(buf), "arma %d", t.weapon);
                dl->AddText(ImVec2(t.scrHead.x - 20.f, y1 + 2.f), colText, buf);
            }

            if (C.espDistance) {
                sprintf_s(buf, sizeof(buf), "%.0fm", (double)t.dist);
                dl->AddText(ImVec2(t.scrHead.x - 16.f, y1 + C.espFontSize + 2.f), colText, buf);
            }

            // ---- snapline "Linha" (base da tela ate os pes) ----
            if (C.espSnapline)
                dl->AddLine(ImVec2(cx, cyBot), ImVec2(t.scrFeet.x, t.scrFeet.y), boxCol, th);

            drawn++;
        }

        SDK::UnlockTargets();
        SDK::dbg_drawn = drawn;
    }

} // namespace ESP