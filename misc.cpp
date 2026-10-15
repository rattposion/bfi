#include "features.h"

namespace Misc {
    void Tick() {
        if (GetAsyncKeyState(C.panicKey) & 1) {
            C.esp = C.aimbot = C.silentAim = C.magicBullets = C.triggerbot = false;
            Log("PANIC: tudo desligado");
        }
    }
    void Draw() {}
}