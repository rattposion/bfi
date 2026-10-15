#include "features.h"
static ULONGLONG g_next = 0;

namespace Triggerbot {
    void Tick() {
        if (!C.triggerbot) return;
        if (GetTickCount64() < g_next) return;
        Il2CppObject* t = Aimbot::Best();
        if (!t) return;
        Il2CppObject* gun = SDK::LocalGun();
        if (!gun) return;
        if (SDK::M_shoot) IL2CPP::Invoke(SDK::M_shoot, gun, nullptr);
        g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay;
    }
    bool Install() { Log("triggerbot instalado"); return true; }
}