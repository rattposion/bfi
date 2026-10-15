#include "features.h"

// contadores visiveis no menu (C.blockedNoZone / C.blockedUnlock)
static bool hk_false(void*, const MethodInfo*) { C.blockedNoZone++; return false; }
static bool hk_true(void*, const MethodInfo*) { C.blockedUnlock++; return true; }

namespace AntiCheat {

    static void hookByName(Il2CppClass* cls, const char* exact, void* det, int* installed) {
        if (!cls) return;
        void* it = nullptr;
        while (const MethodInfo* m = IL2CPP::class_get_methods(cls, &it)) {
            if (!m->methodPointer) continue;
            const char* nm = IL2CPP::method_get_name(m);
            if (!nm || strcmp(nm, exact) != 0) continue;

            // so hooka se o retorno for Boolean (evita quebrar metodos void)
            const Il2CppType* rt = IL2CPP::method_get_return_type(m);
            const char* rtn = rt ? IL2CPP::type_get_name(rt) : nullptr;
            if (!rtn || !strstr(rtn, "Boolean")) continue;

            if (MH_CreateHook((LPVOID)m->methodPointer, det, nullptr) == MH_OK)
                MH_EnableHook((LPVOID)m->methodPointer);
            (*installed)++;
        }
    }

    bool Install() {
        if (!C.antiCheat) return false;

        const char* classes[] = { "PlayerAntiHack", "PlayerAntiCheat", "AntiHack", "AntiCheat" };
        int n = 0, u = 0;

        for (int i = 0; i < 4; i++) {
            Il2CppClass* k = SDK::FindClass(classes[i]);
            if (!k) continue;
            hookByName(k, "IsInsideNoZone", (void*)hk_false, &n);
            hookByName(k, "IsWeaponUnlocked", (void*)hk_true, &u);
        }
        if (SDK::kAntiHack) {
            hookByName(SDK::kAntiHack, "IsInsideNoZone", (void*)hk_false, &n);
            hookByName(SDK::kAntiHack, "IsWeaponUnlocked", (void*)hk_true, &u);
        }

        Log("anti-cheat: nozone=%d unlock=%d", n, u);
        return true;
    }
}