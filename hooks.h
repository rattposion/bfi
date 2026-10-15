#pragma once
#include "common.h"

namespace Hooks {
    extern std::atomic<bool> g_imguiReady;
    extern std::atomic<bool> g_hooksLive;
    extern std::atomic<int>  g_inPresent;
    extern bool g_sdkReady;

    bool InstallSwap();
    bool InstallMainThread();     // LateUpdate/Update -> main thread
    bool InstallCursor();
    bool InstallCombat();         // hurt / localHurt / recoil / FireBullet

    void ApplyFeatures();         // MAIN THREAD
    void RenderFrame();           // RENDER THREAD (so desenha)

    void Shutdown();
    void SaveConfig();
}