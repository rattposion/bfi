// ============================================================================
//  dllmain.cpp  -  versao com log de cada passo (diagnostico)
// ============================================================================

#include <windows.h>
#include <cstdio>

#include "il2cpp.h"
#include "il2cpp_api.h"
#include "common.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"

HMODULE g_hModule = nullptr;
bool    g_unload = false;

void Log(const char* fmt, ...);
void LogInit();

// ---------------------------------------------------------------------------
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    Log("!!!!!!!! EXCECAO code=0x%08X addr=%p fase=%s",
        ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress,
        g_phase);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------------------
static DWORD WINAPI Main(LPVOID) {
    LogInit();

    Log("================================================");
    Log("[bfi] injetado. aguardando o jogo...");
    Log("================================================");

    SetUnhandledExceptionFilter(CrashFilter);

    while (!GetModuleHandleA("GameAssembly.dll")) Sleep(100);
    Log("[bfi] GameAssembly.dll = %p", GetModuleHandleA("GameAssembly.dll"));

    g_phase = "resolve";
    if (!IL2CPP_API::Resolve()) { Log("[bfi] Resolve FALHOU"); return 0; }
    Log("[bfi] API do il2cpp OK");

    g_phase = "domain";
    Il2CppDomain* dom = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (!dom) { Log("[bfi] domain NULO"); return 0; }
    Log("[bfi] dominio il2cpp pronto");

    g_phase = "attach";
    if (il2cpp_thread_attach) il2cpp_thread_attach(dom);
    Log("[bfi] thread attach OK");

    g_phase = "sdk";
    Log("[bfi] >>> chamando SDK::Init...");
    bool okSdk = SDK::Init();
    Log("[bfi] <<< SDK::Init retornou %d", (int)okSdk);

    g_phase = "hooks";
    Log("[bfi] >>> chamando Hooks::Init...");
    bool okHk = Hooks::Init();
    Log("[bfi] <<< Hooks::Init retornou %d", (int)okHk);

    g_phase = "done";
    Log("[bfi] ===== pronto =====");
    return 0;
}

// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = h;
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_unload = true;
        if (g_phase && *g_phase) {
            SDK::Shutdown();
            Hooks::Shutdown();
        }
    }
    return TRUE;
}