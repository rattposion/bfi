// ============================================================================
//  weapons.cpp — stats via SDK (nome + heuristica) e dump completo
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>

namespace Weapons {

    static uint32_t o_cur = 0, o_max = 0, o_reserve = 0;
    static uint32_t o_reload = 0, o_spread = 0, o_rate = 0;
    static Il2CppObject* g_stats = nullptr;
    static bool g_dumped = false;

    void Tick() {
        if (!(C.infAmmo || C.noReload || C.noSpread || C.rapidFire)) return;
        if (!SDK::g_localPlayer) return;

        Il2CppObject* stats = SDK::FindGunStatsObj();
        if (!stats) return;

        if (stats != g_stats) {
            g_stats = stats;
            SDK::ResolveWeaponFields(stats, o_cur, o_max, o_reserve, o_reload, o_spread, o_rate);
            if (!g_dumped) { g_dumped = true; SDK::DumpFieldsWithValues(stats); }
        }

        if (C.infAmmo) {
            if (o_cur) {
                int mx = o_max ? SafeRead<int>((uintptr_t)stats + o_max, 0) : 0;
                SafeWrite((uintptr_t)stats + o_cur, (mx > 0 && mx < 10000) ? mx : 999);
            }
            if (o_reserve) SafeWrite((uintptr_t)stats + o_reserve, 999);
        }
        if (C.noReload && o_reload) SafeWrite<bool>((uintptr_t)stats + o_reload, false);
        if (C.noSpread && o_spread) SafeWrite<float>((uintptr_t)stats + o_spread, 0.f);
        if (C.rapidFire && o_rate)  SafeWrite<float>((uintptr_t)stats + o_rate, 0.01f);
    }

} // namespace Weapons