#include "features.h"
#include <cmath>

namespace Aimbot {

    static Vec3 g_cur(0, 0, 1);
    static Il2CppObject* g_curTf = nullptr;
    static Il2CppClass* g_kQuat = nullptr;
    static bool g_rotTried = false;

    static void EnsureRot() {
        if (g_rotTried) return;
        g_rotTried = true;
        g_kQuat = SDK::FindClass("Quaternion", "UnityEngine");
        Log("aim: quat=%p rotOff=0x%X isQuat=%d",
            (void*)g_kQuat, (unsigned)SDK::O_rotation, SDK::O_rotationIsQuat ? 1 : 0);
    }

    static Il2CppObject* CamTf() {
        Il2CppObject* lp = g_localPlayer;
        if (!lp || !SDK::O_camTf) return nullptr;
        return (Il2CppObject*)SDK::SafeReadObjPub(lp, SDK::O_camTf);
    }

    Vec3 Origin() {
        if (Il2CppObject* t = CamTf()) return SDK::GetPos(t);
        if (g_localPlayer) if (Il2CppObject* tf = SDK::GetTransform(g_localPlayer)) return SDK::GetPos(tf);
        return Vec3();
    }

    Vec3 Bone(Il2CppObject* p) {
        if (!p) return Vec3();
        for (int i = 0; i < g_targetCount; i++) {
            if (g_targets[i].player != p) continue;
            Target& t = g_targets[i];
            Vec3 h = t.headBone ? SDK::GetPos(t.headBone) : t.head;
            Vec3 f = t.footBone ? SDK::GetPos(t.footBone) : t.feet;
            if (C.aimBone == 0) return h;
            if (C.aimBone == 2) return Vec3(h.x, (h.y + f.y) * 0.5f, h.z);
            return Vec3(h.x, h.y - (h.y - f.y) * 0.25f, h.z);
        }
        return Vec3();
    }

    Il2CppObject* Best() {
        if (!g_localPlayer) return nullptr;
        int lt = g_localTeam;
        float cx = SDK::ScreenW() * 0.5f;
        float cy = SDK::ScreenH() * 0.5f;
        float best = 1e18f;
        Il2CppObject* out = nullptr;
        Vec3 eye = Origin();

        for (int i = 0; i < g_targetCount; i++) {
            Il2CppObject* p = g_targets[i].player;
            if (!p || p == g_localPlayer) continue;
            if (C.teamCheck && lt >= 0 && g_targets[i].team == lt) continue;
            if (!g_targets[i].alive) continue;

            Vec3 aim = Bone(p);
            if (aim.zero()) continue;

            Vec2 s;
            if (!SDK::WorldToScreen(aim, s)) continue;
            float dx = s.x - cx, dy = cy - s.y;
            float d2 = dx * dx + dy * dy;
            if (sqrtf(d2) > C.aimFov) continue;
            if (C.aimVisible && !SDK::VisibleLine(eye, aim)) continue;

            float score;
            if (C.aimMode == 2)      score = g_targets[i].dist * g_targets[i].dist;
            else if (C.aimMode == 1) score = (float)g_targets[i].health;
            else                     score = d2;

            if (score < best) { best = score; out = p; }
        }
        return out;
    }

    void WriteAim(Il2CppObject* target) {
        Il2CppObject* camTf = CamTf();
        if (!camTf || !target) return;

        Vec3 point = Bone(target);
        if (C.aimPredict) {
            for (int i = 0; i < g_targetCount; i++) {
                if (g_targets[i].player != target) continue;
                float t = g_targets[i].dist * 0.001f * C.aimPredictMs;
                point.y += 0.02f * t * 60.f;
                break;
            }
        }
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
        Vec3 n(g_cur.x / l2, g_cur.y / l2, g_cur.z / l2);

        // so escreve o campo se ele for realmente Quaternion
        if (SDK::O_rotation && SDK::O_rotationIsQuat && g_kQuat) {
            if (const MethodInfo* lr = IL2CPP::class_get_method_from_name(g_kQuat, "LookRotation", 2)) {
                Vec3 up(0.f, 1.f, 0.f);
                void* a[2] = { &n, &up };
                if (Il2CppObject* r = IL2CPP::Invoke(lr, nullptr, a)) {
                    if (void* u = IL2CPP::object_unbox(r))
                        SafeWrite<Quat>((uintptr_t)g_localPlayer + SDK::O_rotation, *(Quat*)u);
                }
            }
        }
        // reforco: forward da camera
        if (SDK::M_set_forward) {
            void* a[1] = { &n };
            IL2CPP::Invoke(SDK::M_set_forward, (void*)camTf, a);
        }
    }

    void Tick() {
        EnsureRot();

        C.aimKeyDown = (GetAsyncKeyState(C.aimKey) & 0x8000) != 0;
        if (!C.aimbot) { C.aimKeyDown = false; return; }
        if (!C.aimKeyDown) { g_curTf = nullptr; return; }
        if (!g_localPlayer) return;

        // silent/magic sao tratados no hook do FireBullet
        if (C.silentAim || C.magicBullets) return;
        if (!C.aimRotate) return;

        if (Il2CppObject* t = Best()) WriteAim(t);
    }

    bool Install() { return true; }
}