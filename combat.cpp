// ============================================================================
//  combat.cpp — UNICO orquestrador por frame.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

extern "C" int bfi_anticheat_obj;
extern "C" int bfi_weapons_obj;

namespace Combat {

    static uint64_t g_lastDbg = 0;

    void Tick() {
        PHASE("Combat::Tick");
        if (g_unload.load()) return;

        // >>> ANTES do check de viewProj: o unlock precisa rodar tambem no lobby
        PHASE("Combat::Unlock");         Unlock::Tick();

        PHASE("Combat::UpdateView");     SDK::UpdateView();
        if (!SDK::g_viewProjOk) return;

        Hooks::g_sdkReady = true;

        PHASE("Combat::UpdateTargets");  SDK::UpdateTargets();

        PHASE("Combat::Aimbot::Best");
        SDK::g_aimTarget = C.aimbot ? Aimbot::Best() : nullptr;

        PHASE("Combat::Misc");           Misc::Tick();
        PHASE("Combat::Aimbot");         Aimbot::Tick();
        PHASE("Combat::Triggerbot");     Triggerbot::Tick();
        PHASE("Combat::Weapons");        Weapons::Tick();
        PHASE("Combat::Movement");       Movement::Tick();
        PHASE("Combat::AntiCheat");      AntiCheat::Tick();

        uint64_t now = GetTickCount64();
        if (now - g_lastDbg >= 1000) {
            g_lastDbg = now;

            char tb[192]; tb[0] = 0;
            SDK::LockTargets();
            for (int i = 0; i < SDK::g_targetCount && i < 20; ++i) {
                char x[12];
                sprintf_s(x, sizeof(x), "%d,", SDK::g_targets[i].team);
                strcat_s(tb, x);
            }
            SDK::UnlockTargets();

            Log("dbg: alvos=%d localTeam=%d teams=[%s] fase=%s obj=[ac=%d wp=%d]",
                SDK::g_targetCount, SDK::g_localTeam, tb, g_phase ? g_phase : "?",
                bfi_anticheat_obj, bfi_weapons_obj);
        }
    }

} // namespace Combat