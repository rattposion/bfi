#include "common.h"
#include "il2cpp.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

HMODULE g_hModule = nullptr;

namespace f {
    std::atomic<bool> unloadReq{ false };
}

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

static bool WaitModule(const char* name, int seconds) {
    for (int i = 0; i < seconds * 10; i++) {
        if (GetModuleHandleA(name)) return true;
        Sleep(100);
    }
    return false;
}

static bool WaitDomain(int seconds) {
    for (int i = 0; i < seconds * 10; i++) {
        if (IL2CPP::domain_get && IL2CPP::domain_get()) return true;
        Sleep(100);
    }
    return false;
}

static DWORD WINAPI Main(LPVOID) {
    LogInit();
    SetUnhandledExceptionFilter(LogCrash);
    Log("=== nvapi64 iniciando ===");

    if (!WaitModule("GameAssembly.dll", 60)) { Log("GameAssembly.dll nunca apareceu"); return 0; }
    Log("GameAssembly OK, aguardando runtime...");

    if (!IL2CPP::Init()) { Log("IL2CPP::Init FALHOU"); return 0; }

    if (!WaitDomain(60)) { Log("domain il2cpp nunca ficou pronta"); return 0; }
    Log("domain OK");

    Sleep(1500);   // deixa o Assembly-CSharp carregar antes de resolver

    if (!SDK::Init()) { Log("SDK::Init FALHOU"); return 0; }

    ConfigIO::Init();
    Log(">>> STAGE = %d", C.stage);

    if (!Hooks::InstallSwap()) Log("swap falhou");
    Hooks::g_sdkReady = true;
    Hooks::g_hooksLive.store(true);

    if (C.stage >= 2) Hooks::InstallMainThread();
    Hooks::InstallCursor();

    if (C.stage >= 3) {
        Hooks::InstallCombat();
        AntiCheat::Install();          // A1
    }

    Log("=== TUDO PRONTO (stage %d) ===", C.stage);

    // loop do worker: SO input — nada de il2cpp aqui
    bool lastMenu = false, lastPanic = false, lastSave = false, lastUnload = false;
    while (!f::unloadReq.load()) {
        bool menu = (GetAsyncKeyState(C.menuKey) & 0x8000) != 0;
        bool panic = (GetAsyncKeyState(C.panicKey) & 0x8000) != 0;
        bool save = (GetAsyncKeyState(C.saveKey) & 0x8000) != 0;
        bool unload = (GetAsyncKeyState(C.unloadKey) & 0x8000) != 0;

        if (menu && !lastMenu) { C.menuOpen = !C.menuOpen; Log("menu=%d", C.menuOpen ? 1 : 0); }
        if (panic && !lastPanic) { Misc::Panic(); }
        if (save && !lastSave) { ConfigIO::Save(); Log("config salva"); }
        if (unload && !lastUnload) { f::unloadReq.store(true); }

        lastMenu = menu; lastPanic = panic; lastSave = save; lastUnload = unload;
        Sleep(8);
    }

    Hooks::Shutdown();
    ConfigIO::Save();
    Log("=== descarregado ===");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = h;
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
    }
    return TRUE;
}