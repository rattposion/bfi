// recoil.cpp - bfi  (noRecoil aplicado nas armas VIVAS do inventario)
#include "common.h"
#include "sdk.h"
#include "config.h"
#include "features.h"
#include <vector>
#include <cstring>

namespace Recoil {

    struct RF { uint32_t off; bool vec2; };
    static std::vector<RF> g_gun;
    static bool g_installed = false;

    static const char* kRecoil[] = {
        "gunUpKick","gunBackKick","maxGunBackKick","muzzleClimb","muzzleClimbSideways",
        "muzzleClimbDamped","firstShotRecoilMultiplier","cameraBackKick",
        "horizontalRecoil","visualHorizontalRecoil","adsRecoilMult","recoilStability"
    };

    static void Scan()
    {
        g_gun.clear();
        if (!SDK::kGunStats) return;
        void* it = nullptr; FieldInfo* f;
        while ((f = il2cpp_class_get_fields(SDK::kGunStats, &it))) {
            const char* n = il2cpp_field_get_name(f);
            if (!n) continue;
            Il2CppType* t = il2cpp_field_get_type(f);
            const char* tn = t ? il2cpp_type_get_name(t) : nullptr;
            bool isF = tn && strstr(tn, "Single");
            bool isV = tn && strstr(tn, "Vector2");
            if (!isF && !isV) continue;
            for (auto* kn : kRecoil) {
                if (!strcmp(n, kn)) {
                    RF r{}; r.off = (uint32_t)il2cpp_field_get_offset(f); r.vec2 = isV;
                    g_gun.push_back(r);
                    break;
                }
            }
        }
        Log("[recoil] %d campos casados", (int)g_gun.size());
    }

    static void ZeroOn(void* obj)
    {
        if (!obj) return;
        for (auto& r : g_gun) {
            *(float*)((uint8_t*)obj + r.off) = 0.f;
            if (r.vec2) *(float*)((uint8_t*)obj + r.off + 4) = 0.f;
        }
    }

    void Install() { Scan(); g_installed = true; }

    void Tick()
    {
        if (!C.noRecoil) return;
        if (!g_installed || g_gun.empty()) Install();

        void* p = SDK::g_localPlayer;
        if (!p) return;

        Il2CppArray* arr = *(Il2CppArray**)((uint8_t*)p + 0xBA8);
        if (arr) {
            int len = *(int*)((uint8_t*)arr + 0x18);
            for (int i = 0; i < len; i++)
                ZeroOn(*(void**)((uint8_t*)arr + 0x20 + (size_t)i * sizeof(void*)));
        }
        ZeroOn(*(void**)((uint8_t*)p + 0x570));
    }

} // namespace Recoil