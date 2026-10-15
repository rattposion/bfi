#include "features.h"

namespace Weapons {
    void Tick() {
        if (!(C.infAmmo || C.noReload || C.noSpread || C.rapidFire)) return;
        if (!g_localPlayer) return;

        Il2CppObject* stats = nullptr;
        if (SDK::O_gunStats) stats = (Il2CppObject*)SDK::SafeReadObjPub(g_localPlayer, SDK::O_gunStats);
        if (!stats && SDK::O_gunView && SDK::O_gunStatsOnView) {
            Il2CppObject* gun = (Il2CppObject*)SDK::SafeReadObjPub(g_localPlayer, SDK::O_gunView);
            if (gun) stats = (Il2CppObject*)SDK::SafeReadObjPub(gun, SDK::O_gunStatsOnView);
        }
        if (!stats) return;

        if (C.infAmmo && SDK::O_curAmmo && SDK::O_maxAmmo) {
            // mantem o pente cheio sem destruir o maxAmmo
            int mx = SafeRead<int>((uintptr_t)stats + SDK::O_maxAmmo);
            if (mx > 0 && mx < 10000) SafeWrite((uintptr_t)stats + SDK::O_curAmmo, mx);
            else                      SafeWrite((uintptr_t)stats + SDK::O_curAmmo, 999);
        }
        if (C.noReload && SDK::O_reloading) SafeWrite<bool>((uintptr_t)stats + SDK::O_reloading, false);
        if (C.noSpread && SDK::O_spread)    SafeWrite<float>((uintptr_t)stats + SDK::O_spread, 0.f);
        if (C.rapidFire && SDK::O_fireRate) SafeWrite<float>((uintptr_t)stats + SDK::O_fireRate, 0.01f);
    }
}