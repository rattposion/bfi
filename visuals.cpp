#include "features.h"

namespace Visuals {
    void Draw() {
        if (!C.watermark) return;
        ImGuiIO& io = ImGui::GetIO();
        char b[128];
        sprintf_s(b, "Venice BF | %.0f FPS | alvos: %d | silent:%d magic:%d god:%d",
            io.Framerate, g_targetCount, C.silentAim, C.magicBullets, C.godmode);
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(12, 12), IM_COL32(0, 255, 180, 230), b);
    }
}