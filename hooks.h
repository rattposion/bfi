#pragma once
#include "common.h"
#include "imgui.h"
#include <atomic>

namespace Hooks {
    extern std::atomic<bool> g_imguiReady;
    extern std::atomic<bool> g_hooksLive;
    extern std::atomic<bool> g_sdkReady;
    extern std::atomic<uint32_t> g_frameCounter;
    extern int  g_viewW, g_viewH;

    uint32_t FrameId();

    bool InstallSwap();
    bool InstallMainThread();
    bool InstallCursor();
    bool InstallCombat();

    bool Init();
    void Shutdown();
    void ApplyFeatures();
    void HandleKeys();

    ImFont* SafeFont(int idx);
}