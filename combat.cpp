#include "features.h"

// hurt tem 7 parametros -> (self, 7 params, MethodInfo*)
static void (*o_hurt)(void*, void*, void*, void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;
static void hk_hurt(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7, const MethodInfo* m) {
    __try {
        if (C.godmode && self == SDK::LocalPlayer()) return;
        if (C.damageMult && a1) {
            float* f = (float*)a1;
            static DWORD l = 0;
            if (GetTickCount() - l > 500) {
                l = GetTickCount();
                Log("hurt: self=%p a1=%p val=%.2f", self, a1, *f);
            }
            if (*f > 0.f && *f < 100000.f) *f *= C.damageValue;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_hurt(self, a1, a2, a3, a4, a5, a6, a7, m);
}

// localHurt tem 5 parametros -> (self, 5 params, MethodInfo*)
static void (*o_localHurt)(void*, void*, void*, void*, void*, void*, const MethodInfo*) = nullptr;
static void hk_localHurt(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, const MethodInfo* m) {
    __try {
        if (C.godmode && self == SDK::LocalPlayer()) return;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_localHurt(self, a1, a2, a3, a4, a5, m);
}

// ComputeShotRecoil tem 0 parametros -> (self, MethodInfo*)
static void (*o_recoil)(void*, const MethodInfo*) = nullptr;
static void hk_recoil(void* self, const MethodInfo* m) {
    o_recoil(self, m);
}

namespace Combat {
    bool Install() {
        if (SDK::M_hurt && SDK::M_hurt->methodPointer) {
            MH_CreateHook((LPVOID)SDK::M_hurt->methodPointer, hk_hurt, (LPVOID*)&o_hurt);
            MH_EnableHook((LPVOID)SDK::M_hurt->methodPointer);
            Log("hook hurt OK");
        }
        else Log("hook hurt NAO instalado");

        if (SDK::M_localHurt && SDK::M_localHurt->methodPointer) {
            MH_CreateHook((LPVOID)SDK::M_localHurt->methodPointer, hk_localHurt, (LPVOID*)&o_localHurt);
            MH_EnableHook((LPVOID)SDK::M_localHurt->methodPointer);
            Log("hook localHurt OK");
        }
        else Log("hook localHurt NAO instalado");

        if (SDK::M_ComputeRecoil && SDK::M_ComputeRecoil->methodPointer) {
            MH_CreateHook((LPVOID)SDK::M_ComputeRecoil->methodPointer, hk_recoil, (LPVOID*)&o_recoil);
            MH_EnableHook((LPVOID)SDK::M_ComputeRecoil->methodPointer);
            Log("hook recoil OK");
        }
        return true;
    }
    void Tick() {}
}