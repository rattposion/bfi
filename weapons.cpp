// ============================================================================
//  weapons.cpp — offsets resolvidos dinamicamente a partir da classe real
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

namespace Weapons {

    static uint32_t o_cur = 0, o_max = 0, o_reload = 0, o_spread = 0, o_rate = 0;
    static Il2CppClass* g_cls = nullptr;

    static void Ensure(Il2CppClass* k) {
        if (!k || k == g_cls) return;
        g_cls = k;
        o_cur = SDK::PickFieldOff(k, { "currentAmmo", "curAmmo", "ammo", "bullets", "mCurrentAmmo", "magazineAmmo" }, "Int32");
        o_max = SDK::PickFieldOff(k, { "maxAmmo", "ammoMax", "mMaxAmmo", "magazineSize", "clipSize" }, "Int32");
        o_reload = SDK::PickFieldOff(k, { "reloading", "isReloading", "mReloading" }, "Boolean");
        o_spread = SDK::PickFieldOff(k, { "spread", "currentSpread", "mSpread" }, "Single");
        o_rate = SDK::PickFieldOff(k, { "fireRate", "shotDelay", "mFireRate", "cooldown" }, "Single");
        Log("[weapons] stats cls=%s cur=0x%X max=0x%X reload=0x%X spread=0x%X rate=0x%X",
            il2cpp_class_get_name(k), o_cur, o_max, o_reload, o_spread, o_rate);
    }

    void Tick() {
        if (!(C.infAmmo || C.noReload || C.noSpread || C.rapidFire)) return;
        if (!SDK::g_localPlayer) return;

        Il2CppObject* stats = nullptr;
        if (SDK::O_gunStats) stats = (Il2CppObject*)SDK::SafeReadObjPub(SDK::g_localPlayer, SDK::O_gunStats);
        if (!stats && SDK::O_gunView && SDK::O_gunStatsOnView) {
            Il2CppObject* gun = (Il2CppObject*)SDK::SafeReadObjPub(SDK::g_localPlayer, SDK::O_gunView);
            if (gun) stats = (Il2CppObject*)SDK::SafeReadObjPub(gun, SDK::O_gunStatsOnView);
        }
        if (!stats) return;

        Ensure(il2cpp_object_get_class(stats));

        if (C.infAmmo && o_cur) {
            int mx = o_max ? SafeRead<int>((uintptr_t)stats + o_max, 0) : 0;
            SafeWrite((uintptr_t)stats + o_cur, (mx > 0 && mx < 10000) ? mx : 999);
        }
        if (C.noReload && o_reload) SafeWrite<bool>((uintptr_t)stats + o_reload, false);
        if (C.noSpread && o_spread) SafeWrite<float>((uintptr_t)stats + o_spread, 0.f);
        if (C.rapidFire && o_rate)  SafeWrite<float>((uintptr_t)stats + o_rate, 0.01f);
    }

} // namespace Weapons