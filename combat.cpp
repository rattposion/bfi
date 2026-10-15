#include "features.h"

// ---------------------------------------------------------------- hurt (7)
static void (*o_hurt)(void*, void*, void*, void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;

static void hk_hurt(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7, const MethodInfo* m) {
    __try {
        bool isLocal = (self && self == g_localPlayer);
        if (C.godmode && isLocal) return;
        if (!isLocal) f::lastHitTime.store(GetTickCount64());
        if (C.damageMult && a1) {
            float* f = (float*)a1;
            if (*f > 0.f && *f < 100000.f) *f *= C.damageValue;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_hurt(self, a1, a2, a3, a4, a5, a6, a7, m);
}

// ------------------------------------------------------------ localHurt (5)
static void (*o_localHurt)(void*, void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;

static void hk_localHurt(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, const MethodInfo* m) {
    __try { if (C.godmode && self && self == g_localPlayer) return; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_localHurt(self, a1, a2, a3, a4, a5, m);
}

// -------------------------------------------------------------- recoil (4)
// ComputeShotRecoil tem 4 parametros
static void (*o_recoil)(void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;

static void hk_recoil(void* self, void* a1, void* a2, void* a3, void* a4, const MethodInfo* m) {
    __try { if (C.noRecoil) return; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_recoil(self, a1, a2, a3, a4, m);
}

// ------------------------------------------------------------ FireBullet (7)
static void (*o_fire)(void*, void*, void*, void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;

static void hk_fire(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7, const MethodInfo* m) {
    if ((!C.silentAim && !C.magicBullets) || !g_localPlayer) {
        o_fire(self, a1, a2, a3, a4, a5, a6, a7, m);
        return;
    }

    __try {
        Il2CppObject* camTf = nullptr;
        if (SDK::O_camTf) camTf = (Il2CppObject*)SDK::SafeReadObjPub(g_localPlayer, SDK::O_camTf);
        if (!camTf) { o_fire(self, a1, a2, a3, a4, a5, a6, a7, m); return; }

        Quat saved = SDK::GetRot(camTf);

        if (Il2CppObject* target = Aimbot::Best()) {
            Vec3 point = Aimbot::Bone(target);
            if (C.magicBullets) point.y += 0.05f;
            Vec3 eye = SDK::GetPos(camTf);
            Vec3 dir = point - eye;
            float len = dir.len();
            if (len > 0.01f) {
                Vec3 n = dir * (1.0f / len);
                Il2CppClass* kQ = SDK::FindClass("Quaternion", "UnityEngine");
                if (kQ) {
                    if (const MethodInfo* lr = IL2CPP::class_get_method_from_name(kQ, "LookRotation", 2)) {
                        Vec3 up(0.f, 1.f, 0.f);
                        void* args[2] = { &n, &up };
                        if (Il2CppObject* r = IL2CPP::Invoke(lr, nullptr, args)) {
                            if (void* u = IL2CPP::object_unbox(r)) SDK::SetRot(camTf, *(Quat*)u);
                        }
                    }
                }
                if (SDK::M_set_forward) {
                    void* fa[1] = { &n };
                    IL2CPP::Invoke(SDK::M_set_forward, (void*)camTf, fa);
                }
            }
        }

        o_fire(self, a1, a2, a3, a4, a5, a6, a7, m);

        if (C.silentAim) SDK::SetRot(camTf, saved);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { o_fire(self, a1, a2, a3, a4, a5, a6, a7, m); }
}

namespace Combat {
    bool Install() {
        auto hook = [](const MethodInfo* m, void* det, void** orig, const char* label) {
            if (!m || !m->methodPointer) { Log("hook %s NAO resolvido", label); return; }
            if (MH_CreateHook((LPVOID)m->methodPointer, det, orig) != MH_OK) { Log("MH_CreateHook(%s) falhou", label); return; }
            MH_EnableHook((LPVOID)m->methodPointer);
            Log("hook %s OK (%u params)", label, IL2CPP::method_get_param_count(m));
            };
        hook(SDK::M_hurt, (void*)hk_hurt, (void**)&o_hurt, "hurt");
        hook(SDK::M_localHurt, (void*)hk_localHurt, (void**)&o_localHurt, "localHurt");
        hook(SDK::M_ComputeRecoil, (void*)hk_recoil, (void**)&o_recoil, "recoil");
        hook(SDK::M_FireBullet, (void*)hk_fire, (void**)&o_fire, "FireBullet");
        return true;
    }
    void Tick() {}
}