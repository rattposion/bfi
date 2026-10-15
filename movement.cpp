#include "features.h"

namespace Movement {
    void Tick() {
        Il2CppObject* p = SDK::LocalPlayer();
        if (!p) return;
        Il2CppObject* tf = SDK::GetTransform(p);
        if (C.speedHack && SDK::O_speed) FIELD(p, p, SDK::O_speed, float) *= C.speedValue;
        if (C.fly && tf) {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                Vec3 pos = SDK::GetPos(tf);
                pos.y += 0.12f;
                void* a[1] = { &pos };
                IL2CPP::Invoke(SDK::M_set_position, tf, a);
            }
        }
        if (C.teleport && (GetAsyncKeyState('T') & 0x8000)) {
            Il2CppObject* t = Aimbot::Best();
            if (t && tf) {
                Il2CppObject* tt = SDK::GetTransform(t);
                if (tt) {
                    Vec3 pos = SDK::GetPos(tt);
                    void* a[1] = { &pos };
                    IL2CPP::Invoke(SDK::M_set_position, tf, a);
                }
            }
        }
    }
}