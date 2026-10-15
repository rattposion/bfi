#include "features.h"

namespace Misc {

    void Panic() {
        C.esp = C.aimbot = C.silentAim = C.magicBullets = C.triggerbot = false;
        C.godmode = C.speedHack = C.fly = C.teleport = false;
        Log("PANIC: tudo desligado");
    }

    void Tick() {
        if (SDK::M_get_fov && g_localPlayer) {
            // camFov so escreve se o usuario mexeu (evita brigar com ADS)
            static float last = 0.f;
            if (C.camFov > 0.f && C.camFov != last) {
                last = C.camFov;
                float v = C.camFov;
                void* a[1] = { &v };
                Il2CppObject* cam = SDK::CameraObj();
                if (cam) IL2CPP::Invoke(SDK::M_set_fov, cam, a);
            }
        }
    }

    void Draw() {}
}