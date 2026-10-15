// ============================================================================
//  movement.cpp — speedhack, fly, super jump, teleport.
//  Roda na thread principal, chamado pelo hk_lateUpdate.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>

namespace Movement {

    static float g_baseSpeed = 0.f;

    // wrapper unico para set_position(Vector3)
    static void InvokeSetPosition(Il2CppObject* tf, Vec3 pos) {
        if (!SDK::M_set_position || !tf) return;
        void* a[1];
        a[0] = &pos;
        il2cpp_runtime_invoke(SDK::M_set_position, tf, a, nullptr);
    }

    void Tick() {
        if (!SDK::g_localPlayer) return;

        Il2CppObject* tf = SDK::GetTransform(SDK::g_localPlayer);

        // ---- speedhack: guarda o valor base, nao multiplica por frame ----
        if (SDK::O_speed && C.speedHack) {
            if (g_baseSpeed == 0.f)
                g_baseSpeed = ReadAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed, 1.f);

            if (g_baseSpeed > 0.f)
                WriteAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed,
                    g_baseSpeed * C.speedValue);
        }
        else if (SDK::O_speed && !C.speedHack && g_baseSpeed > 0.f) {
            WriteAt<float>((uintptr_t)SDK::g_localPlayer + SDK::O_speed, g_baseSpeed);
            g_baseSpeed = 0.f;
        }

        if (!tf || !SDK::M_set_position) return;

        bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        // ---- fly ----
        if (C.fly && space) {
            Vec3 pos = SDK::GetPos(tf);
            pos.y += 0.12f;
            InvokeSetPosition(tf, pos);
        }

        // ---- super jump ----
        if (C.superJump && space) {
            Vec3 pos = SDK::GetPos(tf);
            pos.y += 0.30f;
            InvokeSetPosition(tf, pos);
        }

        // ---- teleport para o alvo atual ----
        if (C.teleport && (GetAsyncKeyState('T') & 0x8000)) {
            Il2CppObject* t = Aimbot::Best();
            if (t) {
                Il2CppObject* tt = SDK::GetTransform(t);
                if (tt) InvokeSetPosition(tf, SDK::GetPos(tt));
            }
        }
    }

} // namespace Movement