// ============================================================================
//  aimbot.cpp — FOV em GRAUS (convertido p/ pixels), line-of-sight + lostest
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#include <cmath>

#undef near
#undef far

namespace Aimbot {

    static Vec3 g_cur{ 0.f, 0.f, 1.f };
    static Il2CppObject* g_curTf = nullptr;

    static bool  g_wrote = false;
    static bool  g_overwritten = false;
    static bool  g_lastTook = false;
    static Quat  g_lastWritten{};

    // C.aimFov esta em GRAUS -> converte pro raio em pixels na tela
    static float FovRadiusPx() {
        if (C.aimFov <= 0.f) return 1e9f;
        float cot = SDK::CameraCotHalfFovY();
        float halfH = (float)SDK::ScreenH() * 0.5f;
        float t = tanf(C.aimFov * 0.5f * 3.14159265f / 180.f);
        float px = halfH * t * cot;
        if (px < 1.f) px = 1.f;
        if (px > halfH * 2.f) px = halfH * 2.f;
        return px;
    }

    static Il2CppObject* BestFiltered(float fovPx, float& outPx) {
        outPx = 1e9f;
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
            float px = sqrtf(d2);

            if (px < outPx) outPx = px;
            if (px > fovPx) continue;

            if (C.aimVisible && !SDK::TargetVisible(i, eye, aim)) continue;

            float score;
            if (C.aimMode == 2)      score = SDK::g_targets[i].dist * SDK::g_targets[i].dist;
            else if (C.aimMode == 1) score = SDK::g_targets[i].health;
            else                     score = d2;

            if (score < best) { best = score; out = p; }
        }
        return out;
    }

    Il2CppObject* Best() {
        float px;
        return BestFiltered(FovRadiusPx(), px);
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

        SDK::SetRot(camTf, q);
        if (SDK::g_localPlayer) SDK::SetPlayerRot(SDK::g_localPlayer, q);

        Quat rb{};
        g_lastTook = SDK::GetRot(camTf, rb) &&
            (fabsf(rb.x - q.x) + fabsf(rb.y - q.y) + fabsf(rb.z - q.z) + fabsf(rb.w - q.w) < 0.01f);
        g_lastWritten = q;
        g_wrote = true;
    }

    void Tick() {
        if (!C.aimKey) C.aimKeyDown = true;
        else           C.aimKeyDown = (GetAsyncKeyState(C.aimKey) & 0x8000) != 0;

        Il2CppObject* camTfNow = SDK::CamTfObj();

        if (g_wrote && camTfNow && C.aimKeyDown) {
            Quat cur{};
            if (SDK::GetRot(camTfNow, cur)) {
                float d = fabsf(cur.x - g_lastWritten.x) + fabsf(cur.y - g_lastWritten.y) +
                    fabsf(cur.z - g_lastWritten.z) + fabsf(cur.w - g_lastWritten.w);
                g_overwritten = (d > 0.01f);
            }
        }

        static ULONGLONG lastDiag = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastDiag > 1000) {
            lastDiag = now;

            float fovPx = FovRadiusPx();
            float nearPx = -1.f;
            Il2CppObject* nearTgt = BestFiltered(1e9f, nearPx);

            Quat cur{};
            bool gotCur = camTfNow ? SDK::GetRot(camTfNow, cur) : false;
            Vec3 pCamTf = camTfNow ? SDK::GetPos(camTfNow) : Vec3();

            int cLocal = 0, cAlive = 0, cOnScr = 0, cLos = 0, cFov = 0;
            {
                Vec3 eye = camTfNow ? pCamTf : SDK::g_localPos;
                for (int i = 0; i < SDK::g_targetCount; ++i) {
                    Il2CppObject* p = SDK::g_targets[i].player;
                    if (!p) continue;
                    if (p == SDK::g_localPlayer) { cLocal++; continue; }
                    if (!SDK::g_targets[i].alive) continue;
                    cAlive++;
                    if (!SDK::g_targets[i].onScreen) continue;
                    cOnScr++;
                    Vec3 aim = SDK::TargetAimPos(p, C.aimBone);
                    if (aim.zero()) continue;
                    if (!SDK::TargetVisible(i, eye, aim)) continue;
                    cLos++;
                    Vec2 s;
                    if (!SDK::WorldToScreen(aim, s)) continue;
                    float dx = s.x - SDK::ScreenW() * 0.5f;
                    float dy = SDK::ScreenH() * 0.5f - s.y;
                    if (sqrtf(dx * dx + dy * dy) > fovPx) continue;
                    cFov++;
                }
            }

            Log("[aimdiag] on=%d down=%d rotate=%d silent=%d magic=%d key=0x%02X fov=%.1fgraus(%.0fpx) bone=%d alvos=%d",
                (int)C.aimbot, (int)C.aimKeyDown, (int)C.aimRotate, (int)C.silentAim, (int)C.magicBullets,
                C.aimKey, (double)C.aimFov, (double)fovPx, C.aimBone, SDK::g_targetCount);
            Log("[aimdiag] camTf=%p took=%d overwritten=%d near=%p nearPx=%.1f",
                camTfNow, (int)g_lastTook, (int)g_overwritten, nearTgt, (double)nearPx);
            Log("[aimdiag] filtro: alvos=%d local=%d vivos=%d onScreen=%d comLos=%d noFov=%d",
                SDK::g_targetCount, cLocal, cAlive, cOnScr, cLos, cFov);

            // teste cru do raycast (sem tolerancia) no alvo mais proximo
            if (nearTgt && camTfNow) {
                Vec3 eye = SDK::GetPos(camTfNow);
                Vec3 tp = SDK::TargetAimPos(nearTgt, C.aimBone);
                float hit = -1.f;
                bool h = SDK::RaycastTest(eye, tp, hit);
                Log("[aimdiag] lostest: bateu=%d hitDist=%.2f len=%.2f vis=%d",
                    (int)h, (double)hit, (double)(tp - eye).len(),
                    (int)SDK::HasLineOfSight(eye, tp));
            }
        }

        if (!C.aimbot) { C.aimKeyDown = false; return; }
        if (!C.aimKeyDown) { g_curTf = nullptr; return; }
        if (!SDK::g_localPlayer) return;

        if (C.silentAim || C.magicBullets) return;
        if (!C.aimRotate) return;

        if (Il2CppObject* t = Best()) WriteAim(t);
    }

    bool Install() {
        Log("[aimbot] install: camTfOff=0x%X fov=%.1fgraus tecla=0x%02X",
            (unsigned)SDK::O_camTf, (double)C.aimFov, C.aimKey);
        return true;
    }

} // namespace Aimbot