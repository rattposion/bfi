// ============================================================================
//  dllmain.cpp
//
//  >>> CORRECOES:
//   1) IL2CPP::Init() era NUNCA chamado (a camada il2cpp.cpp inteira ficava
//      morta). Agora e chamado e o resultado e logado.
//   2) A tecla de "descarregar" (F8) so setava g_unload e NADA descarregava.
//      Agora uma thread vigia e faz o teardown + FreeLibraryAndExitThread.
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

// ---------------------------------------------------------------------------
//  Vigia do unload. Precisa ser thread PROPRIA: se chamarmos
//  FreeLibraryAndExitThread de dentro do swap hook, matamos a thread de render.
// ---------------------------------------------------------------------------
static DWORD WINAPI UnloadWatch(LPVOID) {
    while (!g_unload.load()) Sleep(50);

    Log("[bfi] descarregando...");
    Sleep(200);            // deixa o frame atual terminar

    Hooks::Shutdown();     // desabilita hooks, restaura wndproc, destroi ImGui
    SDK::Shutdown();

    Log("[bfi] tchau");
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
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

    // >>> FIX: a camada IL2CPP:: (il2cpp.cpp) nunca era inicializada.
    g_phase = "resolve2";
    Log("[bfi] IL2CPP::Init = %d", (int)IL2CPP::Init());

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

    CreateThread(nullptr, 0, UnloadWatch, nullptr, 0, nullptr);
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