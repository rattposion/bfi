// ============================================================================
//  aimbot.cpp — aim na cameraTransform, so em alvo visivel (visual check)
//  >>> FIX: filtro de time sem o guard "!= 0" e unificado com o ESP
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#undef near
#undef far
#include <cmath>

// globais consumidos pelo hooks.cpp (detour FireBullet -> silent/magic)
bool g_silentAimActive = false;
bool g_magicBulletsActive = false;
Vec3 g_silentAimDir{ 0.f, 0.f, 1.f };

namespace Aimbot {

    static Vec3          g_cur{ 0.f, 0.f, 1.f };
    static Il2CppObject* g_curTf = nullptr;

    static float FovRadiusPx(float deg) {
        float cot = SDK::CameraCotHalfFovY();
        float h = (float)SDK::ScreenH();
        if (cot <= 0.f || h <= 0.f) return 99999.f;
        return (h * 0.5f) * tanf(deg * 0.01745329252f) * cot;
    }

    Il2CppObject* Best() {
        if (!SDK::g_localPlayer) return nullptr;

        float cx = SDK::ScreenW() * 0.5f;
        float cy = SDK::ScreenH() * 0.5f;
        float fovPx = FovRadiusPx(C.aimFov);

        float bestScore = 1e18f;
        Il2CppObject* out = nullptr;

        for (int i = 0; i < SDK::g_targetCount; i++) {
            Il2CppObject* p = SDK::g_targets[i].player;
            if (!p || p == SDK::g_localPlayer) continue;
            if (!SDK::g_targets[i].alive) continue;
            // >>> FIX: time 0 tambem filtra; vale pro ESP e pro aim
            if ((C.aimTeam || C.teamCheck) && SDK::g_targets[i].team == SDK::g_localTeam) continue;
            if (C.aimMaxDist > 1.f && SDK::g_targets[i].dist > C.aimMaxDist) continue;

            // visual check: so alvo visivel (verde)
            if ((C.aimVisible || C.aimVisCheck) && !SDK::g_targets[i].visible) continue;

            Vec3 aim = SDK::TargetAimPos(p, C.aimBone);
            if (aim.zero()) continue;

            Vec2 s;
            if (!SDK::WorldToScreen(aim, s)) continue;

            float dx = s.x - cx, dy = cy - s.y;
            float d2 = dx * dx + dy * dy;
            if (sqrtf(d2) > fovPx) continue;

            float score;
            if (C.aimMode == 2)      score = SDK::g_targets[i].dist * SDK::g_targets[i].dist;
            else if (C.aimMode == 1) score = SDK::g_targets[i].health;
            else                     score = d2;

            if (score < bestScore) { bestScore = score; out = p; }
        }
        return out;
    }

    static void WriteAim(Il2CppObject* target) {
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
        if (k < 0.f)    k = 0.f;
        if (k > 0.95f)  k = 0.95f;

        g_cur.x += (want.x - g_cur.x) * k;
        g_cur.y += (want.y - g_cur.y) * k;
        g_cur.z += (want.z - g_cur.z) * k;

        float l2 = g_cur.len();
        if (l2 < 0.0001f) return;
        Vec3 n{ g_cur.x / l2, g_cur.y / l2, g_cur.z / l2 };

        Quat q = SDK::LookRot(n);
        SDK::SetRot(camTf, q);
        if (SDK::g_localPlayer && SDK::O_rotation && SDK::O_rotationIsQuat)
            WriteAt<Quat>((uintptr_t)SDK::g_localPlayer + SDK::O_rotation, q);
    }

    void Tick() {
        C.aimKeyDown = (GetAsyncKeyState(C.aimKey) & 0x8000) != 0;

        if (!C.aimbot) {
            C.aimKeyDown = false;
            SDK::g_aimTarget = nullptr;
            g_silentAimActive = false;
            g_magicBulletsActive = false;
            return;
        }
        if (!SDK::g_localPlayer) return;

        Il2CppObject* t = Best();
        SDK::g_aimTarget = t;

        bool wantSilent = (t != nullptr) && (C.silentAim || C.magicBullets);
        if (wantSilent) {
            Il2CppObject* camTf = SDK::CamTfObj();
            Vec3 point = SDK::TargetAimPos(t, C.aimBone);
            Vec3 eye = camTf ? SDK::GetPos(camTf) : SDK::g_localPos;
            Vec3 d = point - eye;
            float l = d.len();
            if (l > 0.001f) {
                g_silentAimDir = d * (1.0f / l);
                g_silentAimActive = true;
                g_magicBulletsActive = C.magicBullets;
            }
            else g_silentAimActive = false;
        }
        else {
            g_silentAimActive = false;
            g_magicBulletsActive = false;
        }

        if (!C.aimKeyDown) g_curTf = nullptr;

        if (t && C.aimKeyDown && C.aimRotate && !wantSilent)
            WriteAim(t);
    }

    bool Install() {
        Log("[aimbot] install: camTfOff=0x%X fov=%.1f tecla=0x%02X",
            (unsigned)SDK::O_camTf, (double)C.aimFov, C.aimKey);
        return true;
    }

} // namespace Aimbot