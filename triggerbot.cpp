#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#include <vector>

namespace Triggerbot {

    static ULONGLONG g_next = 0;

    static void Fire() {
        if (!SDK::g_localPlayer) return;

        if (SDK::M_shoot) {
            il2cpp_runtime_invoke(SDK::M_shoot, SDK::g_localPlayer, nullptr, nullptr);
            return;
        }
        if (SDK::M_FireBullet) {
            uint32_t n = il2cpp_method_get_param_count(SDK::M_FireBullet);
            if (n == 0) n = 1;
            std::vector<void*> args(n, nullptr);
            il2cpp_runtime_invoke(SDK::M_FireBullet, SDK::g_localPlayer, args.data(), nullptr);
        }
    }

    void Tick() {
        if (!C.triggerbot) return;
        if (!SDK::g_localPlayer) return;
        if (GetTickCount64() < g_next) return;

        if (!SDK::M_shoot && !SDK::M_FireBullet) {
            g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay;
            return;
        }

        Il2CppObject* t = Aimbot::Best();
        if (!t) return;

        Fire();
        C.lastHitTime = (float)(GetTickCount64() / 1000.0);
        g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay;
    }

    bool Install() {
        Log("[triggerbot] install: shoot=%p fire=%p", SDK::M_shoot, SDK::M_FireBullet);
        return true;
    }

} // namespace Triggerbot