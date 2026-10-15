#pragma once
#include "common.h"
#include <atomic>

namespace Hooks {
    extern std::atomic<bool> g_imguiReady;
    extern std::atomic<bool> g_hooksLive;
    extern std::atomic<bool> g_sdkReady;
    extern int  g_viewW, g_viewH;

    bool InstallSwap();
    bool InstallMainThread();
    bool InstallCursor();
    bool InstallCombat();

    bool Init();
    void Shutdown();
    void ApplyFeatures();
    void HandleKeys();
}