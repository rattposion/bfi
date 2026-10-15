// ============================================================================
//  dllmain.cpp
// ============================================================================
#include <windows.h>
#include <atomic>
#include <cstdio>

#include "il2cpp.h"
#include "il2cpp_api.h"
#include "common.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

HMODULE g_hModule = nullptr;
std::atomic<bool> g_unload{ false };

void Log(const char* fmt, ...);
void LogInit();

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    Log("!!!!!!!! EXCECAO code=0x%08X addr=%p fase=%s",
        ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress,
        g_phase ? g_phase : "?");
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD WINAPI Main(LPVOID) {
    LogInit();

    Log("================================================");
    Log("[bfi] injetado. aguardando o jogo...");
    Log("================================================");

    SetUnhandledExceptionFilter(CrashFilter);

    while (!GetModuleHandleA("GameAssembly.dll")) {
        if (g_unload.load()) return 0;
        Sleep(100);
    }
    Log("[bfi] GameAssembly.dll = %p", GetModuleHandleA("GameAssembly.dll"));

    g_phase = "resolve";
    if (!IL2CPP_API::Resolve()) { Log("[bfi] Resolve FALHOU"); return 0; }

    g_phase = "domain";
    Il2CppDomain* dom = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (!dom) { Log("[bfi] domain NULO"); return 0; }

    g_phase = "attach";
    if (il2cpp_thread_attach) il2cpp_thread_attach(dom);

    g_phase = "sdk";
    if (!SDK::Init()) { Log("[bfi] SDK::Init FALHOU"); }

    g_phase = "config";
    ConfigIO::Init();

    g_phase = "hooks";
    if (!Hooks::Init()) { Log("[bfi] Hooks::Init FALHOU"); }

    g_phase = "done";
    Log("[bfi] ===== pronto =====");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = h;
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_unload = true;
        SDK::Shutdown();
        Hooks::Shutdown();
    }
    return TRUE;
}