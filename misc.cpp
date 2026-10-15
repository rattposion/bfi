#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>

namespace Misc {

    void Panic() {
        C.esp = false;
        C.aimbot = false;
        C.silentAim = false;
        C.magicBullets = false;
        C.triggerbot = false;
        C.godmode = false;
        C.speedHack = false;
        C.fly = false;
        C.teleport = false;
        C.crosshair = false;
        C.noRecoil = false;
        Log("PANIC: tudo desligado");
    }

    void Tick() {
        static float last = -1.f;
        if (C.camFov <= 0.f) { last = -1.f; return; }
        if (C.camFov == last) return;
        if (!SDK::M_set_fov) return;

        Il2CppObject* cam = SDK::CameraObj();
        if (!cam) return;

        last = C.camFov;
        float v = C.camFov;
        void* a[1];
        a[0] = &v;
        il2cpp_runtime_invoke(SDK::M_set_fov, cam, a, nullptr);
    }

} // namespace Misc