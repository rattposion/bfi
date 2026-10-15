#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#ifndef PHASE
#define PHASE(x) (g_phase = (x))
#endif

namespace Combat {

    static uint64_t g_lastDbg = 0;

    void Tick() {
        PHASE("Combat::Tick");

        // ---- tudo aqui roda na MAIN THREAD ----
        SDK::GetCamera();
        SDK::RefreshTargets();      // FindObjectsOfType no maximo 4x/s
        SDK::UpdateTargets();       // pos/HP/osso/projecao por alvo

        PHASE("Aimbot::Tick");      Aimbot::Tick();
        PHASE("Triggerbot::Tick");  Triggerbot::Tick();
        PHASE("Weapons::Tick");     Weapons::Tick();
        PHASE("Movement::Tick");    Movement::Tick();
        PHASE("Misc::Tick");        Misc::Tick();
        PHASE("Anticheat::Tick");   AntiCheat::Tick();

        // ---- debug 1x/s ----
        uint64_t now = GetTickCount64();
        if (now - g_lastDbg >= 1000) {
            g_lastDbg = now;
            auto snap = SDK::Snapshot();
            int alvos = 0, vivos = 0, comBone = 0, naTela = 0;
            for (auto& t : snap) {
                if (!t.obj) continue;
                alvos++;
                if (t.alive) vivos++;
                if (t.headBone) comBone++;
                if (t.onScreen) naTela++;
            }
            Log("dbg: alvos=%d vivos=%d comBone=%d naTela=%d local=%p cam=%p fase=%s",
                alvos, vivos, comBone, naTela, SDK::g_local, SDK::g_camera,
                g_phase ? g_phase : "?");
        }
    }

} // namespace Combat