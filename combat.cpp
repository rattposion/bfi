// ============================================================================
//  combat.cpp — UNICO orquestrador por frame. (v4)
//
//  >>> FIX 1 (o "nada funciona"): a ordem era Unlock -> UpdateView ->
//  "if (!g_viewProjOk) return;" -> UpdateTargets. Camera indisponivel cortava
//  TUDO. O scan de alvos NAO depende da camera e o return nao existe mais.
//
//  >>> FIX 2: __try por passo. Um AV num passo nao mata o frame.
//
//  >>> FIX 3: PROFILER. Cada passo e' cronometrado e exposto em Combat::g_stats
//  para o overlay (Mostrar > Profiler). Use isso pra achar o proximo gargalo.
//
//  >>> FIX 4: Aimbot::Best() so roda se aimbot OU triggerbot estiver ligado.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

extern "C" int bfi_anticheat_obj;
extern "C" int bfi_weapons_obj;

namespace Combat {

    static uint64_t g_lastDbg = 0;

    StepStat g_stats[kMaxSteps];
    int      g_statsN = 0;
    float    g_tickMs = 0.f;

    static double QpcMs() {
        static double inv = 0.0;
        if (inv == 0.0) {
            LARGE_INTEGER f; QueryPerformanceFrequency(&f);
            inv = (f.QuadPart > 0) ? (1000.0 / (double)f.QuadPart) : 0.0;
        }
        LARGE_INTEGER c; QueryPerformanceCounter(&c);
        return (double)c.QuadPart * inv;
    }

    static void Stat(const char* nome, double ms) {
        for (int i = 0; i < g_statsN; ++i) {
            if (strcmp(g_stats[i].name, nome) != 0) continue;
            g_stats[i].ms = g_stats[i].ms * 0.90f + (float)ms * 0.10f;
            g_stats[i].last = (float)ms;
            return;
        }
        if (g_statsN < kMaxSteps) {
            strncpy_s(g_stats[g_statsN].name, nome, _TRUNCATE);
            g_stats[g_statsN].ms = (float)ms;
            g_stats[g_statsN].last = (float)ms;
            ++g_statsN;
        }
    }

#define BFI_STEP(nome, chamada)                     \
    PHASE("Combat::" nome);                         \
    {                                               \
        double t0 = QpcMs();                        \
        __try { chamada; }                          \
        __except (EXCEPTION_EXECUTE_HANDLER) {      \
            Log("[crash] passo %s", nome);          \
        }                                           \
        Stat(nome, QpcMs() - t0);                   \
    }

    void Tick() {
        PHASE("Combat::Tick");
        if (g_unload.load()) return;

        const double t0 = QpcMs();

        BFI_STEP("Unlock", Unlock::Tick());
        BFI_STEP("UpdateTargets", SDK::UpdateTargets());
        BFI_STEP("UpdateView", SDK::UpdateView());

        Hooks::g_sdkReady = true;

        BFI_STEP("AimbotBest",
            SDK::g_aimTarget = (C.aimbot || C.triggerbot) ? Aimbot::Best() : nullptr);
        BFI_STEP("Misc", Misc::Tick());
        BFI_STEP("Aimbot", Aimbot::Tick());
        BFI_STEP("Triggerbot", Triggerbot::Tick());
        BFI_STEP("Weapons", Weapons::Tick());
        BFI_STEP("Movement", Movement::Tick());
        BFI_STEP("AntiCheat", AntiCheat::Tick());

        g_tickMs = g_tickMs * 0.9f + (float)(QpcMs() - t0) * 0.1f;

        uint64_t now = GetTickCount64();
        if (now - g_lastDbg >= 1000) {
            g_lastDbg = now;
            Log("dbg: alvos=%d local=%p team=%d viewProj=%d tick=%.2fms fase=%s obj=[ac=%d wp=%d]",
                SDK::g_targetCount, SDK::g_localPlayer, SDK::g_localTeam,
                (int)SDK::g_viewProjOk, (double)g_tickMs, g_phase ? g_phase : "?",
                bfi_anticheat_obj, bfi_weapons_obj);
        }
    }

#undef BFI_STEP

} // namespace Combat