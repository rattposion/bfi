// ============================================================================
//  hooks.cpp — versao final consolidada
//  - dedupe por instancia (self) + por frame
//  - AimLock salva o ORIGINAL, RestoreAim devolve
//  - recoil com retorno (nao deixa XMM0 lixo)
//  - sdkReady so apos viewProj valido
//  - WndProc consome input com menu aberto
//  - uninit unico do MinHook
//  - todos os ticks ligados
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <GL/gl.h>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

#include "il2cpp.h"
#include "il2cpp_api.h"
#include "common.h"
#include "memory.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

#pragma comment(lib, "opengl32.lib")

namespace Hooks {

    decltype(g_hooksLive) g_hooksLive = false;
    decltype(g_sdkReady)  g_sdkReady = false;
    decltype(g_viewW)     g_viewW = 1920;
    decltype(g_viewH)     g_viewH = 1080;

    static bool g_mhInit = false;
    static std::atomic<uint32_t> g_frameCounter{ 0 };
    static uint32_t g_lastTickFrame = 0xFFFFFFFF;

    static bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

    static bool Edge(int vk) {
        static bool prev[256] = {};
        bool cur = KeyDown(vk);
        bool r = cur && !prev[vk & 0xFF];
        prev[vk & 0xFF] = cur;
        return r;
    }

    static MethodInfo* FindMethodByArity(Il2CppClass* k, const char* name, uint32_t argc) {
        if (!k || !name) return nullptr;
        void* iter = nullptr;
        for (;;) {
            const MethodInfo* m = il2cpp_class_get_methods(k, &iter);
            if (!m) break;
            if (il2cpp_method_get_param_count(m) == argc &&
                strcmp(il2cpp_method_get_name(m), name) == 0)
                return (MethodInfo*)m;
        }
        return nullptr;
    }

    static MethodInfo* FindMethodAny(Il2CppClass* k, const char* name) {
        if (!k || !name) return nullptr;
        void* iter = nullptr;
        for (;;) {
            const MethodInfo* m = il2cpp_class_get_methods(k, &iter);
            if (!m) break;
            if (strcmp(il2cpp_method_get_name(m), name) == 0) return (MethodInfo*)m;
        }
        return nullptr;
    }

    static void* FindMethodPtr(const char* klass, const char* method, uint32_t argc) {
        Il2CppClass* k = SDK::FindClass(klass);
        if (!k) return nullptr;
        MethodInfo* m = FindMethodByArity(k, method, argc);
        return m ? SDK::MethodPtr(m) : nullptr;
    }

    // -----------------------------------------------------------------------
    //  cursor
    // -----------------------------------------------------------------------
    typedef BOOL(WINAPI* tSetCursorPos)(int, int);
    static tSetCursorPos o_setCursorPos = nullptr;

    static BOOL WINAPI hk_SetCursorPos(int x, int y) {
        if (C.menuOpen) return TRUE;
        return o_setCursorPos ? o_setCursorPos(x, y) : TRUE;
    }

    bool InstallCursor() {
        HMODULE u32 = GetModuleHandleA("user32.dll");
        if (!u32) return false;
        void* p = (void*)GetProcAddress(u32, "SetCursorPos");
        if (!p) return false;
        if (MH_CreateHook(p, &hk_SetCursorPos, (void**)&o_setCursorPos) != MH_OK) return false;
        MH_EnableHook(p);
        Log("[hooks] cursor hook OK");
        return true;
    }

    // -----------------------------------------------------------------------
    //  WNDPROC + render
    // -----------------------------------------------------------------------
    static HWND    g_hwnd = nullptr;
    static WNDPROC o_wndProc = nullptr;
    static bool    g_imguiInit = false;
    static bool    g_frameLog = false;

    static LRESULT CALLBACK hk_wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (g_imguiInit) {
            ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp);
            if (C.menuOpen) {
                ImGuiIO& io = ImGui::GetIO();
                if (io.WantCaptureMouse || io.WantCaptureKeyboard) return TRUE;
            }
        }
        return CallWindowProcA(o_wndProc, h, msg, wp, lp);
    }

    typedef BOOL(WINAPI* tSwapBuffers)(HDC);
    static tSwapBuffers o_swap = nullptr;

    static void InitImGui(HWND hwnd) {
        if (g_imguiInit) return;
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        if (!ImGui_ImplWin32_Init(hwnd)) { Log("[hooks] ImGui_ImplWin32_Init FALHOU"); return; }
        if (!ImGui_ImplOpenGL3_Init("#version 130")) { Log("[hooks] ImGui_ImplOpenGL3_Init FALHOU"); return; }
        g_imguiInit = true;
        Log("[hooks] ImGui inicializado (hwnd=%p)", hwnd);
    }

    static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
        g_frameCounter.fetch_add(1, std::memory_order_relaxed);

        HWND hwnd = WindowFromDC(hdc);
        if (!hwnd) hwnd = GetForegroundWindow();

        if (hwnd && hwnd != g_hwnd) {
            g_hwnd = hwnd;
            WNDPROC old = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)hk_wndProc);
            if (old) o_wndProc = old;
            Log("[hooks] wndproc instalado (hwnd=%p)", hwnd);
            InitImGui(hwnd);
        }

        static ULONGLONG lastBeat = 0;
        ULONGLONG nb = GetTickCount64();
        if (nb - lastBeat > 2000) {
            lastBeat = nb;
            Log("[hooks] beat: menu=%d sdk=%d alvos=%d local=%p",
                (int)C.menuOpen, (int)g_sdkReady, SDK::g_targetCount, SDK::g_localPlayer);
        }

        if (Edge(VK_INSERT)) C.menuOpen = !C.menuOpen;
        if (C.aimKey) C.aimKeyDown = KeyDown(C.aimKey);

        if (!g_imguiInit) return o_swap(hdc);

        if (!g_frameLog) {
            g_frameLog = true;
            Log("[hooks] primeiro frame render OK (menuOpen=%d, sdkReady=%d)",
                (int)C.menuOpen, (int)g_sdkReady);
        }

        RECT rc;
        GetClientRect(g_hwnd, &rc);
        g_viewW = (int)(rc.right - rc.left);
        g_viewH = (int)(rc.bottom - rc.top);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();
        if (g_sdkReady) {
            ESP::Draw();
            Misc::Draw();
        }

        ImGui::Render();
        glViewport(0, 0, g_viewW, g_viewH);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        return o_swap(hdc);
    }

    bool InstallSwap() {
        HMODULE gl = GetModuleHandleA("opengl32.dll");
        if (!gl) { Log("[hooks] opengl32.dll nao carregada"); return false; }
        void* p = (void*)GetProcAddress(gl, "wglSwapBuffers");
        if (!p) { Log("[hooks] wglSwapBuffers NAO encontrado"); return false; }
        if (MH_CreateHook(p, &hk_wglSwapBuffers, (void**)&o_swap) != MH_OK) {
            Log("[hooks] MH_CreateHook(wglSwapBuffers) FALHOU");
            return false;
        }
        MH_EnableHook(p);
        Log("[hooks] wglSwapBuffers hook OK");
        return true;
    }

    // -----------------------------------------------------------------------
    //  main thread
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tLateUpdate)(void* self, void* method);
    static tLateUpdate o_lateUpdate = nullptr;

    static void __fastcall hk_lateUpdate(void* self, void* method) {
        if (o_lateUpdate) o_lateUpdate(self, method);

        // 1) filtro primario: so a instancia do local player
        if (SDK::g_localPlayer && self != SDK::g_localPlayer) return;

        // 2) backup: nunca mais de uma vez por frame
        uint32_t f = g_frameCounter.load(std::memory_order_relaxed);
        if (f == g_lastTickFrame) return;
        g_lastTickFrame = f;

        SDK::g_frameId = f;

        SDK::UpdateView();
        if (!SDK::g_viewProjOk) return;

        g_sdkReady = true;

        SDK::UpdateTargets();

        if (C.aimbot) SDK::g_aimTarget = Aimbot::Best();
        else          SDK::g_aimTarget = nullptr;

        Misc::Tick();
        Combat::Tick();
        Aimbot::Tick();
        Triggerbot::Tick();
        Weapons::Tick();
        Movement::Tick();
        AntiCheat::Tick();
    }

    bool InstallMainThread() {
        static const char* classes[] = { "PlayerScript", "GameManager", "Game", "HUD", "UIManager",
                                         "GameController", "Main", "ClientInstance" };
        static const char* methods[] = { "LateUpdate", "Update", "FixedUpdate" };

        for (size_t c = 0; c < sizeof(classes) / sizeof(classes[0]); ++c) {
            Il2CppClass* k = SDK::FindClass(classes[c]);
            if (!k) continue;
            for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); ++m) {
                void* ptr = FindMethodPtr(classes[c], methods[m], 0);
                if (!ptr) continue;
                if (MH_CreateHook(ptr, &hk_lateUpdate, (void**)&o_lateUpdate) == MH_OK) {
                    MH_EnableHook(ptr);
                    Log("[hooks] MAIN THREAD = %s::%s (%p)", classes[c], methods[m], ptr);
                    return true;
                }
            }
        }
        Log("[hooks] main thread: NENHUM alvo encontrado");
        return false;
    }

    // -----------------------------------------------------------------------
    //  combate
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tShoot)(void* self, const MethodInfo* m);
    static tShoot o_shoot = nullptr;

    static void __fastcall hk_shoot(void* self, const MethodInfo* m) {
        Quat original{};
        bool has = false;

        if (C.aimbot && C.aimKeyDown && SDK::g_aimTarget &&
            SDK::g_localPlayer && SDK::O_rotationIsQuat) {
            // AimLock salva o ORIGINAL em `original` e aplica a mira
            if (SDK::AimLock(SDK::g_aimTarget, original)) has = true;
        }

        if (o_shoot) o_shoot(self, m);

        if (has && SDK::g_localPlayer) SDK::RestoreAim(SDK::g_localPlayer, original);
    }

    typedef Quat(__fastcall* tRecoil)(void* self, const MethodInfo* m,
        void* a1, void* a2, void* a3, void* a4);
    static tRecoil o_recoil = nullptr;

    static Quat __fastcall hk_recoil(void* self, const MethodInfo* m,
        void* a1, void* a2, void* a3, void* a4) {
        const Quat zero{ 0.f, 0.f, 0.f, 1.f };
        if (C.noRecoil) return zero;
        return o_recoil ? o_recoil(self, m, a1, a2, a3, a4) : zero;
    }

    bool InstallCombat() {
        bool ok = false;

        if (SDK::M_shoot) {
            void* p = SDK::MethodPtr(SDK::M_shoot);
            if (p && MH_CreateHook(p, &hk_shoot, (void**)&o_shoot) == MH_OK) {
                MH_EnableHook(p);
                Log("[hooks] hook shoot OK");
                ok = true;
            }
        }
        else Log("[hooks] M_shoot nulo");

        // recoil: aridade desconhecida -> varredura
        if (!SDK::M_recoil) {
            Il2CppClass* ps = SDK::FindClass("PlayerScript");
            if (ps) {
                for (uint32_t a = 0; a <= 6 && !SDK::M_recoil; ++a)
                    SDK::M_recoil = FindMethodByArity(ps, "ComputeShotRecoil", a);
                if (!SDK::M_recoil) SDK::M_recoil = FindMethodAny(ps, "ComputeShotRecoil");
            }
        }

        if (SDK::M_recoil) {
            Log("[hooks] recoil: params=%u", il2cpp_method_get_param_count(SDK::M_recoil));
            void* p = SDK::MethodPtr(SDK::M_recoil);
            if (p && MH_CreateHook(p, &hk_recoil, (void**)&o_recoil) == MH_OK) {
                MH_EnableHook(p);
                Log("[hooks] hook recoil OK");
                ok = true;
            }
        }
        else Log("[hooks] M_recoil nulo");

        return ok;
    }

    // -----------------------------------------------------------------------
    bool Init() {
        if (g_mhInit) return true;
        if (MH_Initialize() != MH_OK) { Log("[hooks] MH_Initialize FALHOU"); return false; }
        g_mhInit = true;

        InstallSwap();
        InstallMainThread();
        InstallCursor();
        InstallCombat();
        AntiCheat::Install();

        g_hooksLive = true;
        Log("[hooks] instalados");
        return true;
    }

    void Shutdown() {
        if (!g_mhInit) return;
        g_sdkReady = false;
        g_hooksLive = false;

        if (o_wndProc && g_hwnd) {
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)o_wndProc);
            o_wndProc = nullptr;
        }
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_mhInit = false;
        Log("[hooks] Shutdown");
    }

} // namespace Hooks