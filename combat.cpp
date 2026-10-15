// ============================================================================
//  combat.cpp — UNICO orquestrador por frame.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

namespace Combat {

    static uint64_t g_lastDbg = 0;

    void Tick() {
        PHASE("Combat::Tick");
        if (g_unload.load()) return;

        SDK::UpdateView();
        if (!SDK::g_viewProjOk) return;

        Hooks::g_sdkReady = true;

        SDK::UpdateTargets();                       // ja refresca alvos a cada 500ms

        SDK::g_aimTarget = C.aimbot ? Aimbot::Best() : nullptr;

        Misc::Tick();
        Aimbot::Tick();
        Triggerbot::Tick();
        Weapons::Tick();
        Movement::Tick();
        AntiCheat::Tick();

        uint64_t now = GetTickCount64();
        if (now - g_lastDbg >= 1000) {
            g_lastDbg = now;
            Log("dbg: alvos=%d vivos? local=%p cam=%p fase=%s",
                SDK::g_targetCount, SDK::g_localPlayer, SDK::g_camera, g_phase ? g_phase : "?");
        }
    }

} // namespace Combat