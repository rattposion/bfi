// ============================================================================
//  movement.cpp — speedhack, fly, super jump, teleport
//  >>> FIX: usa SDK::g_aimTarget em vez de chamar Aimbot::Best() de novo.
//  >>> FIX C4244: ReadAt<T>(base, off) tem 2 params -> o "1.f" era lido como
//      OFFSET (uint32_t) e nao como valor padrao. A leitura ia pra +1 byte.
//      Agora usa o overload de 1 argumento.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>

namespace Movement {

    static float g_baseSpeed = 0.f;
    static ULONGLONG g_last = 0;

    static void InvokeSetPosition(Il2CppObject* tf, Vec3 pos) {
        if (!SDK::M_set_position || !tf) return;
        void* a[1];
        a[0] = &pos;
        il2cpp_runtime_invoke(SDK::M_set_position, tf, a, nullptr);
    }

    void Tick() {
        ULONGLONG now = GetTickCount64();
        float dt = g_last ? (float)(now - g_last) / 1000.f : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        g_last = now;

        if (!SDK::g_localPlayer) return;

        Il2CppObject* tf = SDK::GetTransform(SDK::g_localPlayer);

        if (SDK::O_speed && C.speedHack) {
            if (g_baseSpeed == 0.f)
                g_baseSpeed = ReadAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed);
            if (g_baseSpeed > 0.f)
                WriteAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed, g_baseSpeed * C.speedValue);
        }
        else if (SDK::O_speed && !C.speedHack && g_baseSpeed > 0.f) {
            WriteAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed, g_baseSpeed);
            g_baseSpeed = 0.f;
        }

        if (!tf || !SDK::M_set_position) return;

        bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        if (C.fly && space) {
            Vec3 pos = SDK::GetPos(tf);
            pos.y += C.flySpeed * dt;
            InvokeSetPosition(tf, pos);
        }

        if (C.superJump && space) {
            Vec3 pos = SDK::GetPos(tf);
            pos.y += C.jumpPower * dt * 0.5f;
            InvokeSetPosition(tf, pos);
        }

        if (C.teleport && (GetAsyncKeyState('T') & 0x8000)) {
            Il2CppObject* t = SDK::g_aimTarget;
            if (t) {
                Il2CppObject* tt = SDK::GetTransform(t);
                if (tt) InvokeSetPosition(tf, SDK::GetPos(tt));
            }
        }
    }

} // namespace Movement