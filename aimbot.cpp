// ============================================================================
//  aimbot.cpp — aim agora escreve na rotacao da cameraTransform
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#include <cmath>

namespace Aimbot {

    static Vec3 g_cur{ 0.f, 0.f, 1.f };
    static Il2CppObject* g_curTf = nullptr;

    Il2CppObject* Best() {
        if (!SDK::g_localPlayer) return nullptr;

        int lt = SDK::g_localTeam;
        float cx = SDK::ScreenW() * 0.5f;
        float cy = SDK::ScreenH() * 0.5f;
        float best = 1e18f;
        Il2CppObject* out = nullptr;
        Il2CppObject* camTf = SDK::CamTfObj();
        Vec3 eye = camTf ? SDK::GetPos(camTf) : SDK::g_localPos;

        for (int i = 0; i < SDK::g_targetCount; i++) {
            Il2CppObject* p = SDK::g_targets[i].player;
            if (!p || p == SDK::g_localPlayer) continue;
            if (C.aimTeam && lt != 0 && SDK::g_targets[i].team == lt) continue;
            if (!SDK::g_targets[i].alive) continue;
            if (C.aimMaxDist > 1.f && SDK::g_targets[i].dist > C.aimMaxDist) continue;

            Vec3 aim = SDK::TargetAimPos(p, C.aimBone);
            if (aim.zero()) continue;

            Vec2 s;
            if (!SDK::WorldToScreen(aim, s)) continue;

            float dx = s.x - cx, dy = cy - s.y;
            float d2 = dx * dx + dy * dy;
            if (sqrtf(d2) > C.aimFov) continue;
            if (C.aimVisible && !SDK::VisibleLine(eye, aim)) continue;

            float score;
            if (C.aimMode == 2)      score = SDK::g_targets[i].dist * SDK::g_targets[i].dist;
            else if (C.aimMode == 1) score = SDK::g_targets[i].health;
            else                     score = d2;

            if (score < best) { best = score; out = p; }
        }
        return out;
    }

    void WriteAim(Il2CppObject* target) {
        Il2CppObject* camTf = SDK::CamTfObj();
        if (!camTf || !target) return;

        Vec3 point = SDK::TargetAimPos(target, C.aimBone);
        if (point.zero()) return;

        Vec3 eye = SDK::GetPos(camTf);
        Vec3 dir = point - eye;
        float len = dir.len();
        if (len < 0.01f) return;
        Vec3 want = dir * (1.0f / len);

        if (g_curTf != camTf) { g_curTf = camTf; g_cur = want; }

        float k = 1.0f - 1.0f / (C.aimSmooth > 0.f ? C.aimSmooth : 1.f);
        if (k < 0.f) k = 0.f;
        if (k > 0.95f) k = 0.95f;

        g_cur.x += (want.x - g_cur.x) * k;
        g_cur.y += (want.y - g_cur.y) * k;
        g_cur.z += (want.z - g_cur.z) * k;

        float l2 = g_cur.len();
        if (l2 < 0.0001f) return;
        Vec3 n{ g_cur.x / l2, g_cur.y / l2, g_cur.z / l2 };

        Quat q = SDK::LookRot(n);

        // 1) camera (define a mira / crosshair)
        SDK::SetRot(camTf, q);

        // 2) corpo
        if (SDK::g_localPlayer && SDK::O_rotation && SDK::O_rotationIsQuat)
            WriteAt<Quat>((uintptr_t)SDK::g_localPlayer + SDK::O_rotation, q);
    }

    void Tick() {
        C.aimKeyDown = (GetAsyncKeyState(C.aimKey) & 0x8000) != 0;

        if (!C.aimbot) { C.aimKeyDown = false; return; }
        if (!C.aimKeyDown) { g_curTf = nullptr; return; }
        if (!SDK::g_localPlayer) return;

        if (C.silentAim || C.magicBullets) return;
        if (!C.aimRotate) return;

        if (Il2CppObject* t = Best()) WriteAim(t);
    }

    bool Install() {
        Log("[aimbot] install: camTfOff=0x%X fov=%.1f tecla=0x%02X",
            (unsigned)SDK::O_camTf, (double)C.aimFov, C.aimKey);
        return true;
    }

} // namespace Aimbot