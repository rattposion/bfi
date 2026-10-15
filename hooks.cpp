#include "hooks.h"
#include "features.h"
#include "sdk.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"

namespace Hooks {

    bool g_imguiReady = false;
    bool g_sdkReady = false;
    bool g_ready = false;
    bool g_lateHooked = false;

    static WNDPROC   g_wnd = nullptr;
    static HWND      g_hwnd = nullptr;
    static ULONGLONG g_lastPrune = 0;

    // ---------------------------------------------------------------- LateUpdate
    static void (*o_LateUpdate)(void*, const MethodInfo*) = nullptr;

    static void hk_LateUpdate(void* self, const MethodInfo* m) {
        __try { if (o_LateUpdate) o_LateUpdate(self, m); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        Il2CppObject* p = (Il2CppObject*)self;
        if (p && p->klass) {
            __try { AddTarget(p); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // snapshot de alvos na main thread - evita GC no render thread
        static ULONGLONG lastScan = 0;
        if (GetTickCount64() - lastScan > 50) {
            lastScan = GetTickCount64();
            __try { SDK::ScanPlayers(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    // --------------------------------------------------------- cursor (o jogo trava)
    static void (*o_setLockState)(int, const MethodInfo*) = nullptr;
    static void hk_setLockState(int mode, const MethodInfo* m) {
        if (C.menuOpen) mode = 0;
        if (o_setLockState) o_setLockState(mode, m);
    }

    static void (*o_setVisible)(bool, const MethodInfo*) = nullptr;
    static void hk_setVisible(bool v, const MethodInfo* m) {
        if (C.menuOpen) v = true;
        if (o_setVisible) o_setVisible(v, m);
    }

    static void ForceCursorFree() {
        if (!C.menuOpen) return;
        if (SDK::M_set_lockState) { int v = 0; void* a[1] = { &v }; IL2CPP::Invoke(SDK::M_set_lockState, nullptr, a); }
        if (SDK::M_set_visible) { bool v = true; void* a[1] = { &v }; IL2CPP::Invoke(SDK::M_set_visible, nullptr, a); }
    }

    // -------------------------------------------------------------------- WndProc
    static LRESULT CALLBACK hkWnd(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (g_imguiReady) {
            ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp);

            if (C.menuOpen) {
                switch (msg) {
                case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN:
                case WM_MBUTTONUP: case WM_MOUSEWHEEL:
                    ForceCursorFree();
                    return 0;
                }
            }
        }
        return CallWindowProcW(g_wnd, h, msg, wp, lp);
    }

    // ----------------------------------------------------------------------- tick
    void Tick() {
        if (!g_sdkReady) return;

        SDK::dbg_local = SDK::LocalPlayer();

        ForceCursorFree();

        if (GetTickCount64() - g_lastPrune > 250) { g_lastPrune = GetTickCount64(); PruneTargets(); }

        Weapons::Tick();
        Combat::Tick();
        Movement::Tick();
        Aimbot::Tick();
        Triggerbot::Tick();
        Misc::Tick();

        static ULONGLONG lastDbg = 0;
        if (GetTickCount64() - lastDbg > 2000) {
            lastDbg = GetTickCount64();
            Log("dbg: local=%p alvos=%d scan=%d w2s=%d rej=%d adds=%d",
                (void*)SDK::LocalPlayer(), g_targetCount, SDK::dbg_scanCount,
                SDK::dbg_ok_w2s, SDK::dbg_reject, SDK::dbg_adds);
        }
    }

    // --------------------------------------------------------------------- render
    void RenderFrame() {
        if (!g_imguiReady) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = C.menuOpen;

        if (g_sdkReady) {
            __try { Tick(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            __try { ESP::Draw(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            __try { Visuals::Draw(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            __try { Misc::Draw(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        ::MenuDraw();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // --------------------------------------------------------------- swap buffers
    static BOOL(WINAPI* o_wglSwap)(HDC) = nullptr;

    static BOOL WINAPI hk_wglSwap(HDC hdc) {
        if (!g_imguiReady) {
            g_hwnd = WindowFromDC(hdc);
            if (g_hwnd) {
                g_wnd = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hkWnd);
                ImGui::CreateContext();
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                ImGui::StyleColorsDark();
                ImGui_ImplWin32_Init(g_hwnd);
                ImGui_ImplOpenGL3_Init(nullptr);
                g_imguiReady = true;
                Log("imgui inicializado");
            }
        }
        __try { RenderFrame(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return o_wglSwap(hdc);
    }

    // --------------------------------------------------------------------- install
    bool InstallSwap() {
        if (MH_Initialize() != MH_OK) Log("MH_Initialize falhou");

        HMODULE gl = GetModuleHandleA("opengl32.dll");
        if (!gl) { Log("opengl32.dll NAO carregada"); return false; }

        void* p = (void*)GetProcAddress(gl, "wglSwapBuffers");
        if (!p) { Log("wglSwapBuffers NAO achado"); return false; }

        if (MH_CreateHook(p, hk_wglSwap, (LPVOID*)&o_wglSwap) != MH_OK) { Log("MH_CreateHook(swap) falhou"); return false; }
        MH_EnableHook(p);
        Log("hook wglSwapBuffers OK");
        g_ready = true;
        return true;
    }

    bool InstallLateUpdate() {
        if (!SDK::M_LateUpdate || !SDK::M_LateUpdate->methodPointer) { Log("LateUpdate NAO resolvido"); return false; }
        if (MH_CreateHook((LPVOID)SDK::M_LateUpdate->methodPointer, hk_LateUpdate, (LPVOID*)&o_LateUpdate) != MH_OK)
            Log("MH_CreateHook(LateUpdate) falhou");
        MH_EnableHook((LPVOID)SDK::M_LateUpdate->methodPointer);
        g_lateHooked = true;
        Log("hook LateUpdate OK");
        return true;
    }

    bool InstallCursor() {
        bool ok = false;

        if (SDK::M_set_lockState && SDK::M_set_lockState->methodPointer) {
            if (MH_CreateHook((LPVOID)SDK::M_set_lockState->methodPointer, hk_setLockState, (LPVOID*)&o_setLockState) == MH_OK)
                MH_EnableHook((LPVOID)SDK::M_set_lockState->methodPointer);
            ok = true;
        }
        if (SDK::M_set_visible && SDK::M_set_visible->methodPointer) {
            if (MH_CreateHook((LPVOID)SDK::M_set_visible->methodPointer, hk_setVisible, (LPVOID*)&o_setVisible) == MH_OK)
                MH_EnableHook((LPVOID)SDK::M_set_visible->methodPointer);
            ok = true;
        }

        Log("hook cursor: lock=%s vis=%s",
            (SDK::M_set_lockState && SDK::M_set_lockState->methodPointer) ? "OK" : "FALHOU",
            (SDK::M_set_visible && SDK::M_set_visible->methodPointer) ? "OK" : "FALHOU");
        return ok;
    }

} // namespace Hooks