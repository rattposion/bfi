#include "features.h"

static ULONGLONG g_next = 0;

namespace Triggerbot {
    void Tick() {
        if (!C.triggerbot) return;
        if (GetTickCount64() < g_next) return;
        if (!g_localPlayer) return;

        Il2CppObject* t = Aimbot::Best();
        if (!t) return;

        const MethodInfo* m = SDK::M_shoot ? SDK::M_shoot : SDK::M_FireBullet;
        if (!m) { g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay; return; }

        // M_shoot agora e SemiAutoShoot(0) -> chamado no player local
        IL2CPP::Invoke(m, (void*)g_localPlayer, nullptr);

        g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay;
    }
    bool Install() { Log("triggerbot ok"); return true; }
}