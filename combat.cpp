// ============================================================================
//  combat.cpp — UNICO orquestrador por frame. (v3)
//
//  >>> FIX 1 (o "nada funciona"):
//  A ordem era Unlock -> UpdateView -> "if (!g_viewProjOk) return;" ->
//  UpdateTargets. Quando a camera ficava indisponivel (entrada de partida,
//  troca de cena), esse return cortava TUDO: sem scan de alvos (alvos=0 para
//  sempre), sem aimbot, sem ESP, sem weapons. Era exatamente o que o log
//  mostrava: as linhas "dbg:" param e "alvos=0" congela.
//  Agora o scan de alvos NAO depende da camera e nao existe mais o return.
//
//  >>> FIX 2: __try por passo. Um AV num passo nao mata o frame nem o
//  LateUpdate do jogo (e o [veh] loga, mas o resto continua).
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

extern "C" int bfi_anticheat_obj;
extern "C" int bfi_weapons_obj;

namespace Combat {

    static uint64_t g_lastDbg = 0;

#define BFI_STEP(nome, chamada)                     \
    PHASE("Combat::" nome);                         \
    __try { chamada; }                              \
    __except (EXCEPTION_EXECUTE_HANDLER) {          \
        Log("[crash] passo %s", nome);              \
    }

    void Tick() {
        PHASE("Combat::Tick");
        if (g_unload.load()) return;

        // NAO depende de camera / viewProj
        BFI_STEP("Unlock", Unlock::Tick());
        BFI_STEP("UpdateTargets", SDK::UpdateTargets());
        BFI_STEP("UpdateView", SDK::UpdateView());

        Hooks::g_sdkReady = true;

        BFI_STEP("Aimbot::Best", SDK::g_aimTarget = C.aimbot ? Aimbot::Best() : nullptr);
        BFI_STEP("Misc", Misc::Tick());
        BFI_STEP("Aimbot", Aimbot::Tick());
        BFI_STEP("Triggerbot", Triggerbot::Tick());
        BFI_STEP("Weapons", Weapons::Tick());
        BFI_STEP("Movement", Movement::Tick());
        BFI_STEP("AntiCheat", AntiCheat::Tick());

        uint64_t now = GetTickCount64();
        if (now - g_lastDbg >= 1000) {
            g_lastDbg = now;
            Log("dbg: alvos=%d local=%p team=%d viewProj=%d fase=%s obj=[ac=%d wp=%d]",
                SDK::g_targetCount, SDK::g_localPlayer, SDK::g_localTeam,
                (int)SDK::g_viewProjOk, g_phase ? g_phase : "?",
                bfi_anticheat_obj, bfi_weapons_obj);
        }
    }

#undef BFI_STEP

} // namespace Combat