#pragma once
#include "common.h"

namespace Hooks {
    extern bool g_imguiReady;
    extern bool g_sdkReady;
    extern bool g_ready;
    extern bool g_lateHooked;

    bool InstallSwap();
    bool InstallLateUpdate();
    bool InstallCursor();
    void Tick();
    void RenderFrame();
}