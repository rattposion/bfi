// ============================================================================
//  visuals.cpp
//  >>> FIX: o circulo do FOV usava uma formula propria com constante fixa
//  (0.4142 = tan(22.5 graus)) enquanto o aim usava a matriz de projecao da
//  camera. Os dois nao batiam (~2.4x de diferenca). Agora ambos usam
//  SDK::FovRadiusPx.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <imgui.h>
#include <cmath>
#include <cstdio>

static inline ImU32 C4(const float c[4], float aMul = 1.f) {
    return IM_COL32((int)(c[0] * 255.f), (int)(c[1] * 255.f),
        (int)(c[2] * 255.f), (int)(c[3] * 255.f * aMul));
}

static void DrawWatermark(ImDrawList* dl) {
    char buf[128];
    sprintf_s(buf, sizeof(buf), "bfi  |  alvos %d  |  desenhados %d",
        SDK::g_targetCount, SDK::dbg_drawn);
    dl->AddText(ImVec2(12.f, 10.f), IM_COL32(255, 255, 255, 230), buf);
}

static void DrawFps(ImDrawList* dl) {
    static ULONGLONG last = 0;
    static float fps = 0.f;
    ULONGLONG now = GetTickCount64();
    if (last) {
        ULONGLONG dt = now - last;
        if (dt > 0) fps = fps * 0.9f + (1000.f / (float)dt) * 0.1f;
    }
    last = now;
    char buf[32];
    sprintf_s(buf, sizeof(buf), "%.0f fps", (double)fps);
    dl->AddText(ImVec2(12.f, 30.f), IM_COL32(180, 220, 180, 220), buf);
}

static void DrawCrosshair(ImDrawList* dl, float cx, float cy) {
    float s = C.crosshairSize;
    float g = 2.f;
    ImU32 c = C4(C.crosshairColor);
    dl->AddLine(ImVec2(cx - s - g, cy), ImVec2(cx - g, cy), c, 1.5f);
    dl->AddLine(ImVec2(cx + g, cy), ImVec2(cx + s + g, cy), c, 1.5f);
    dl->AddLine(ImVec2(cx, cy - s - g), ImVec2(cx, cy - g), c, 1.5f);
    dl->AddLine(ImVec2(cx, cy + g), ImVec2(cx, cy + s + g), c, 1.5f);
}

static void DrawFovCircle(ImDrawList* dl, float cx, float cy, float H) {
    if (!C.showFovCircle) return;
    if (!SDK::g_viewProjOk) return;

    float r = SDK::FovRadiusPx(C.aimFov);   // >>> FIX: mesma formula do aim
    if (r < 3.f || r > H) return;
    dl->AddCircle(ImVec2(cx, cy), r, C4(C.aimColor, 0.8f), 64, 1.5f);
}

static void DrawHitmarker(ImDrawList* dl, float cx, float cy) {
    if (!C.hitmarker) return;
    float now = (float)(GetTickCount64() / 1000.0);
    float dt = now - C.lastHitTime;
    if (dt < 0.f || dt > 0.25f) return;
    float a = 1.f - dt / 0.25f;
    float s = 8.f + dt * 20.f;
    ImU32 c = IM_COL32(255, 255, 255, (int)(255.f * a));
    dl->AddLine(ImVec2(cx - s, cy - s), ImVec2(cx - s * 0.4f, cy - s * 0.4f), c, 2.f);
    dl->AddLine(ImVec2(cx + s, cy - s), ImVec2(cx + s * 0.4f, cy - s * 0.4f), c, 2.f);
    dl->AddLine(ImVec2(cx - s, cy + s), ImVec2(cx - s * 0.4f, cy + s * 0.4f), c, 2.f);
    dl->AddLine(ImVec2(cx + s, cy + s), ImVec2(cx + s * 0.4f, cy + s * 0.4f), c, 2.f);
}

namespace Misc {

    void Draw() {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        float W = (float)SDK::ScreenW();
        float H = (float)SDK::ScreenH();
        float cx = W * 0.5f;
        float cy = H * 0.5f;

        if (C.watermark) { DrawWatermark(dl); if (C.fps) DrawFps(dl); }
        if (C.crosshair) DrawCrosshair(dl, cx, cy);
        DrawFovCircle(dl, cx, cy, H);
        DrawHitmarker(dl, cx, cy);
    }

} // namespace Misc