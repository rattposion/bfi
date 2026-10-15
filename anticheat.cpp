#include "features.h"

static bool hk_false(void* self, const MethodInfo* m) { C.blockedNoZone++; return false; }
static bool hk_true(void* self, const MethodInfo* m) { C.blockedUnlock++; return true; }

namespace AntiCheat {
    bool Install() {
        if (!C.antiCheat) return false;

        Il2CppClass* list[5] = {
            SDK::kAntiHack,
            SDK::FindClass("PlayerAntiCheat"),
            SDK::FindClass("AntiHack"),
            SDK::FindClass("PlayerAntiHackScript"),
            SDK::FindClass("AntiCheat")
        };

        int n = 0, u = 0;
        for (auto cls : list) {
            if (!cls) continue;
            void* it = nullptr;
            const MethodInfo* m = nullptr;
            while ((m = IL2CPP::class_get_methods(cls, &it))) {
                if (!m->methodPointer) continue;
                const char* nm = IL2CPP::method_get_name(m);
                if (!nm) continue;
                if (strstr(nm, "NoZone")) {
                    MH_CreateHook((LPVOID)m->methodPointer, hk_false, nullptr);
                    MH_EnableHook((LPVOID)m->methodPointer);
                    n++;
                }
                else if (strstr(nm, "Unlock")) {
                    MH_CreateHook((LPVOID)m->methodPointer, hk_true, nullptr);
                    MH_EnableHook((LPVOID)m->methodPointer);
                    u++;
                }
            }
        }
        Log("anti-cheat: nozone=%d unlock=%d", n, u);
        return true;
    }
}