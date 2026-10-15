#include "features.h"

namespace Weapons {
    void Tick() {
        if (!(C.infAmmo || C.noReload || C.noSpread || C.rapidFire)) return;
        Il2CppObject* p = SDK::LocalPlayer();
        if (!p) return;
        Il2CppObject* stats = FIELD(p, p, SDK::O_gunStats, Il2CppObject*);
        if (!stats) {
            Il2CppObject* gun = FIELD(p, p, SDK::O_gunView, Il2CppObject*);
            if (gun) stats = FIELD(gun, gun, SDK::O_gunStats, Il2CppObject*);
        }
        if (!stats) return;
        if (C.infAmmo && SDK::O_curAmmo) FIELD(stats, stats, SDK::O_curAmmo, int) = 999;
        if (C.noReload && SDK::O_reloading) FIELD(stats, stats, SDK::O_reloading, bool) = false;
        if (C.noSpread && SDK::O_spread) FIELD(stats, stats, SDK::O_spread, float) = 0.f;
        if (C.rapidFire && SDK::O_fireRate) FIELD(stats, stats, SDK::O_fireRate, float) = 0.01f;
    }
}