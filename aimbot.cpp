#include "features.h"
#include <cmath>

namespace Aimbot {

    static Vec3 g_cur(0, 0, 1);
    static Il2CppObject* g_curTf = nullptr;

    static Il2CppClass* g_kQuat = nullptr;
    static const MethodInfo* g_lookRot = nullptr;
    static const MethodInfo* g_lookRot2 = nullptr;
    static bool               g_rotTried = false;

    static void EnsureRot() {
        if (g_rotTried) return;
        g_rotTried = true;

        g_kQuat = SDK::FindClass("Quaternion", "UnityEngine");
        if (g_kQuat) {
            g_lookRot = IL2CPP::class_get_method_from_name(g_kQuat, "LookRotation", 1);
            g_lookRot2 = IL2CPP::class_get_method_from_name(g_kQuat, "LookRotation", 2);
        }

        const char* tn = "?";
        Il2CppClass* kp = SDK::kPlayer;
        if (kp) {
            void* f = IL2CPP::class_get_field_from_name(kp, "rotation");
            if (f) {
                const Il2CppType* ft = IL2CPP::field_get_type(f);
                tn = ft ? IL2CPP::type_get_name(ft) : "?";
            }
        }
        Log("aim: LookRotation(1)=%s LookRotation(2)=%s rotOff=0x%X tipo=%s",
            g_lookRot ? "OK" : "FALHOU", g_lookRot2 ? "OK" : "FALHOU",
            (unsigned)SDK::O_rotation, tn ? tn : "?");
    }

    static Il2CppObject* CamTf() {
        Il2CppObject* lp = SDK::LocalPlayer();
        if (!lp || !SDK::O_camTf) return nullptr;
        return (Il2CppObject*)SDK::SafeReadObjPub(lp, SDK::O_camTf);
    }

    Vec3 Origin() {
        Il2CppObject* t = CamTf();
        if (t) return SDK::GetPos(t);
        Il2CppObject* lp = SDK::LocalPlayer();
        if (lp) {
            Il2CppObject* tf = SDK::GetTransform(lp);
            if (tf) return SDK::GetPos(tf);
        }
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
        Il2CppObject* local = SDK::LocalPlayer();
        if (!local) return nullptr;

        int lt = SDK::GetTeam(local);
        float cx = SDK::ScreenW() * 0.5f;
        float cy = SDK::ScreenH() * 0.5f;
        float best = 1e18f;
        Il2CppObject* out = nullptr;
        Vec3 eye = Origin();

        static DWORD lastDbg = 0;
        bool dbg = (GetTickCount() - lastDbg) > 1000;
        if (dbg) lastDbg = GetTickCount();

        int nTotal = 0, nNoBone = 0, nNoW2S = 0, nForaFov = 0, nInvis = 0;

        for (int i = 0; i < g_targetCount; i++) {
            Il2CppObject* p = g_targets[i].player;
            if (!p || p == local) continue;
            if (C.teamCheck && lt >= 0 && g_targets[i].team == lt) continue;
            if (!g_targets[i].alive) continue;
            nTotal++;

            Vec3 aim = Bone(p);
            if (aim.x == 0.f && aim.y == 0.f && aim.z == 0.f) { nNoBone++; continue; }

            Vec2 s;
            if (!SDK::WorldToScreen(aim, s)) { nNoW2S++; continue; }
            float dx = s.x - cx;
            float dy = cy - s.y;
            float d2 = dx * dx + dy * dy;
            if (sqrtf(d2) > C.aimFov) { nForaFov++; continue; }

            if (C.aimVisible && !SDK::VisibleLine(eye, aim)) { nInvis++; continue; }

            float score;
            if (C.aimMode == 2)      score = g_targets[i].dist * g_targets[i].dist;
            else if (C.aimMode == 1) score = (float)g_targets[i].health;
            else                     score = d2;

            if (score < best) { best = score; out = p; }
        }

        if (dbg) {
            Log("best: total=%d noBone=%d noW2S=%d foraFov=%d invis=%d -> %s",
                nTotal, nNoBone, nNoW2S, nForaFov, nInvis, out ? "ACHOU" : "NENHUM");
        }
        return out;
    }

    static void WriteRotation(Il2CppObject* lp, Il2CppObject* camTf, const Vec3& dir) {
        float len = dir.len();
        if (len < 0.01f) return;
        Vec3 nrm = dir * (1.0f / len);

        if (g_lookRot2) {
            Vec3 up = Vec3(0.f, 1.f, 0.f);
            void* a[2] = { (void*)&nrm, (void*)&up };
            Il2CppObject* r = nullptr;
            __try { r = IL2CPP::Invoke(g_lookRot2, nullptr, a); }
            __except (EXCEPTION_EXECUTE_HANDLER) { r = nullptr; }
            if (r) {
                void* u = nullptr;
                __try { u = IL2CPP::object_unbox(r); }
                __except (EXCEPTION_EXECUTE_HANDLER) { u = nullptr; }
                if (u) {
                    Quat q = *(Quat*)u;
                    __try { *(Quat*)((uintptr_t)lp + SDK::O_rotation) = q; }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        }
        else if (g_lookRot && SDK::O_rotation && lp) {
            void* a[1] = { (void*)&nrm };
            Il2CppObject* r = nullptr;
            __try { r = IL2CPP::Invoke(g_lookRot, nullptr, a); }
            __except (EXCEPTION_EXECUTE_HANDLER) { r = nullptr; }
            if (r) {
                void* u = nullptr;
                __try { u = IL2CPP::object_unbox(r); }
                __except (EXCEPTION_EXECUTE_HANDLER) { u = nullptr; }
                if (u) {
                    Quat q = *(Quat*)u;
                    __try { *(Quat*)((uintptr_t)lp + SDK::O_rotation) = q; }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        }

        if (SDK::M_set_forward && camTf) {
            void* a[1] = { (void*)&nrm };
            IL2CPP::Invoke(SDK::M_set_forward, (void*)camTf, a);
        }
    }

    void Tick() {
        static ULONGLONG lastLog = 0;
        EnsureRot();

        C.aimKeyDown = (GetAsyncKeyState(C.aimKey) & 0x8000) != 0;

        if (!C.aimbot) { C.aimKeyDown = false; return; }
        if (!C.aimKeyDown) { g_curTf = nullptr; return; }

        Il2CppObject* local = SDK::LocalPlayer();
        if (!local) return;

        Il2CppObject* camTf = CamTf();
        if (!camTf) return;

        Il2CppObject* target = Best();

        bool logNow = (GetTickCount64() - lastLog > 1000);
        if (logNow) {
            lastLog = GetTickCount64();
            if (!target) Log("aim: sem alvo (alvos=%d visivel=%d fov=%.0f key=0x%X)",
                g_targetCount, C.aimVisible ? 1 : 0, C.aimFov, C.aimKey);
        }
        if (!target) return;

        Vec3 point = Bone(target);
        Vec3 eye = SDK::GetPos(camTf);
        Vec3 dir = point - eye;
        float len = dir.len();
        if (len < 0.01f) return;
        Vec3 want = dir * (1.0f / len);

        if (g_curTf != camTf) { g_curTf = camTf; g_cur = want; }

        float k = 1.0f - 1.0f / (C.aimSmooth > 0.f ? C.aimSmooth : 1.f);
        if (k < 0.f) k = 0.f; if (k > 0.95f) k = 0.95f;

        g_cur.x += (want.x - g_cur.x) * k;
        g_cur.y += (want.y - g_cur.y) * k;
        g_cur.z += (want.z - g_cur.z) * k;

        float l2 = sqrtf(g_cur.x * g_cur.x + g_cur.y * g_cur.y + g_cur.z * g_cur.z);
        if (l2 < 0.0001f) return;
        Vec3 n(g_cur.x / l2, g_cur.y / l2, g_cur.z / l2);

        WriteRotation(local, camTf, n);

        if (logNow) Log("aim: alvo=%p dist=%.1f dir=%.2f %.2f %.2f", (void*)target, len, n.x, n.y, n.z);
    }

    bool Install() { return true; }

} // namespace Aimbot