#include "common.h"
#include "il2cpp.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

HMODULE g_hModule = nullptr;

static LONG WINAPI LogCrash(EXCEPTION_POINTERS* ep) {
    Log("!!!!!!!! EXCECAO !!!!!!!!");
    Log("codigo   = 0x%08X", ep->ExceptionRecord->ExceptionCode);
    Log("endereco = %p", ep->ExceptionRecord->ExceptionAddress);
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &mod) && mod) {
        char path[MAX_PATH]{};
        GetModuleFileNameA(mod, path, MAX_PATH);
        Log("modulo   = %s + 0x%llX", path,
            (unsigned long long)((uintptr_t)ep->ExceptionRecord->ExceptionAddress - (uintptr_t)mod));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD WINAPI Main(LPVOID) {
    LogInit();
    SetUnhandledExceptionFilter(LogCrash);
    Log("=== nvapi64 iniciando ===");

    while (!GetModuleHandleA("GameAssembly.dll")) Sleep(100);
    Log("GameAssembly OK, aguardando...");
    Sleep(3000);

    if (!IL2CPP::Init()) { Log("IL2CPP::Init FALHOU"); return 0; }
    if (!SDK::Init()) { Log("SDK::Init FALHOU"); return 0; }

    ConfigIO::Init();
    Log(">>> STAGE = %d", C.stage);

    Hooks::InstallSwap();
    Hooks::g_sdkReady = true;

    if (C.stage >= 2) Hooks::InstallLateUpdate();
    Hooks::InstallCursor();
    if (C.stage >= 3) { Combat::Install(); AntiCheat::Install(); }

    Hooks::g_ready = true;
    Log("=== TUDO PRONTO (stage %d) ===", C.stage);

    while (true) Sleep(1000);
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = h;
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
    }
    return TRUE;
}