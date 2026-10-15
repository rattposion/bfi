#include "features.h"

namespace Movement {
    static float g_baseSpeed = 0.f;

    void Tick() {
        if (!g_localPlayer) return;
        Il2CppObject* tf = SDK::GetTransform(g_localPlayer);

        // nao multiplica por frame: guarda o valor base
        if (SDK::O_speed) {
            if (C.speedHack) {
                if (g_baseSpeed == 0.f)
                    g_baseSpeed = SafeRead<float>((uintptr_t)g_localPlayer + SDK::O_speed, 1.f);
                if (g_baseSpeed > 0.f)
                    SafeWrite<float>((uintptr_t)g_localPlayer + SDK::O_speed, g_baseSpeed * C.speedValue);
            }
            else if (g_baseSpeed > 0.f) {
                SafeWrite<float>((uintptr_t)g_localPlayer + SDK::O_speed, g_baseSpeed);
                g_baseSpeed = 0.f;
            }
        }

        if (C.fly && tf && SDK::M_set_position) {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                Vec3 pos = SDK::GetPos(tf);
                pos.y += 0.12f;
                void* a[1] = { &pos };
                IL2CPP::Invoke(SDK::M_set_position, tf, a);
            }
        }

        if (C.superJump && tf && SDK::M_set_position) {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                Vec3 pos = SDK::GetPos(tf);
                pos.y += 0.30f;
                void* a[1] = { &pos };
                IL2CPP::Invoke(SDK::M_set_position, tf, a);
            }
        }

        if (C.teleport && tf && SDK::M_set_position && (GetAsyncKeyState('T') & 0x8000)) {
            if (Il2CppObject* t = Aimbot::Best()) {
                if (Il2CppObject* tt = SDK::GetTransform(t)) {
                    Vec3 pos = SDK::GetPos(tt);
                    void* a[1] = { &pos };
                    IL2CPP::Invoke(SDK::M_set_position, tf, a);
                }
            }
        }
    }
}