// ============================================================================
//  triggerbot.cpp
//
//  >>> CORRECOES:
//   1) O fallback para FireBullet passava TODOS os argumentos como nullptr.
//      FireBullet tem parametros por valor (Vector3/Single/...) e o
//      runtime_invoke DESREFERENCIA o ponteiro -> lixo ou AV. Agora aloca
//      um buffer por parametro, como FireBulletAt faz.
//   2) Usa SDK::g_aimTarget (alvo unico do frame) em vez de Aimbot::Best().
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#include <vector>
#include <cstring>

namespace Triggerbot {

    static ULONGLONG g_next = 0;

    static void Fire() {
        if (!SDK::g_localPlayer) return;

        if (SDK::M_shoot) {
            il2cpp_runtime_invoke(SDK::M_shoot, SDK::g_localPlayer, nullptr, nullptr);
            return;
        }
        if (!SDK::M_FireBullet) return;

        uint32_t n = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(SDK::M_FireBullet) : 0;
        if (n == 0) return;

        // >>> FIX: buffers por parametro (antes: std::vector<void*>(n, nullptr))
        std::vector<Vec3>          v(n);
        std::vector<float>         f(n, 0.f);
        std::vector<int32_t>       i(n, 0);
        std::vector<double>        d(n, 0.0);
        std::vector<unsigned char> b(n, 0);
        std::vector<void*>         args(n, nullptr);

        for (uint32_t k = 0; k < n; ++k) {
            const Il2CppType* t = il2cpp_method_get_param ? il2cpp_method_get_param(SDK::M_FireBullet, k) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (!tn) continue;
            if (strstr(tn, "Vector3"))      args[k] = &v[k];
            else if (strstr(tn, "Single"))  args[k] = &f[k];
            else if (strstr(tn, "Int32"))   args[k] = &i[k];
            else if (strstr(tn, "Double"))  args[k] = &d[k];
            else if (strstr(tn, "Boolean")) args[k] = &b[k];
        }

        il2cpp_runtime_invoke(SDK::M_FireBullet, SDK::g_localPlayer, args.data(), nullptr);
    }

    void Tick() {
        if (!C.triggerbot) return;
        if (!SDK::g_localPlayer) return;
        if (GetTickCount64() < g_next) return;

        if (!SDK::M_shoot && !SDK::M_FireBullet) {
            g_next = GetTickCount64() + (ULONGLONG)C.triggerDelay;
            return;
        }

        Il2CppObject* t = SDK::g_aimTarget;   // >>> FIX: alvo unico do frame
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