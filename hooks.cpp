#include "hooks.h"
#include "features.h"
#include "sdk.h"
#include "config.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
#include <GL/gl.h>

#pragma comment(lib, "opengl32.lib")

// ---------------------------------------------------------------------------
// O <GL/gl.h> do Windows so cobre GL 1.1. FBO e GL 3.0 -> carrega via wglGetProcAddress.
// ---------------------------------------------------------------------------
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING  0x8CA6
#endif

typedef void (APIENTRY* PFN_glBindFramebuffer_t)(GLenum target, GLuint framebuffer);
static PFN_glBindFramebuffer_t p_glBindFramebuffer = nullptr;

namespace Hooks {

    std::atomic<bool> g_imguiReady{ false };
    std::atomic<bool> g_hooksLive{ false };
    std::atomic<int>  g_inPresent{ 0 };
    bool g_sdkReady = false;

    static WNDPROC   g_wnd = nullptr;
    static HWND      g_hwnd = nullptr;
    static ULONGLONG g_lastPrune = 0;
    static ULONGLONG g_lastApply = 0;

    static void* g_swapAddr = nullptr;

    // ---------------------------------------------------------------- GL ext
    static void LoadGLExt() {
        if (p_glBindFramebuffer) return;
        p_glBindFramebuffer = (PFN_glBindFramebuffer_t)wglGetProcAddress("glBindFramebuffer");
        if (!p_glBindFramebuffer) {
            HMODULE gl = GetModuleHandleA("opengl32.dll");
            if (gl) p_glBindFramebuffer = (PFN_glBindFramebuffer_t)GetProcAddress(gl, "glBindFramebuffer");
        }
        Log("glBindFramebuffer = %p", (void*)p_glBindFramebuffer);
    }

    // ========================================================================
    // MAIN THREAD
    // ========================================================================
    void ApplyFeatures() {
        if (!g_sdkReady || !g_hooksLive.load()) return;

        ULONGLONG now = GetTickCount64();
        if (now - g_lastApply < 16) return;      // ~60Hz
        g_lastApply = now;

        g_localPlayer = SDK::LocalPlayer();
        g_localTeam = g_localPlayer ? SDK::GetTeam(g_localPlayer) : -1;
        g_localPos = g_localPlayer ? SDK::GetPos(SDK::GetTransform(g_localPlayer)) : Vec3();
        g_inGame = g_localPlayer != nullptr;
        g_camera = SDK::CameraObj();

        if (now - g_lastPrune > 250) { g_lastPrune = now; PruneTargets(); }

        __try { ScanAllTargets(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try { Weapons::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Combat::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Movement::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Aimbot::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Triggerbot::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Misc::Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        static ULONGLONG lastDbg = 0;
        if (now - lastDbg > 3000) {
            lastDbg = now;
            int vivos = 0;
            for (int i = 0; i < g_targetCount; i++) if (g_targets[i].alive) vivos++;
            Log("dbg: local=%p cam=%p alvos=%d vivos=%d desenhados=%d scan=%d rej=%d",
                (void*)g_localPlayer, (void*)g_camera, g_targetCount, vivos,
                SDK::dbg_drawn, SDK::dbg_scanCount, SDK::dbg_reject);
        }
    }

    static void (*o_MainTick)(void*, const MethodInfo*) = nullptr;
    static void hk_MainTick(void* self, const MethodInfo* m) {
        __try { if (o_MainTick) o_MainTick(self, m); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { ApplyFeatures(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // ========================================================================
    // CURSOR
    // ========================================================================
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
        if (o_setLockState) { int  v = 0;    o_setLockState(v, nullptr); }
        if (o_setVisible) { bool v = true; o_setVisible(v, nullptr); }
    }

    // ========================================================================
    // WNDPROC
    // ========================================================================
    static LRESULT CALLBACK hkWnd(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (g_imguiReady.load()) {
            ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp);
            if (C.menuOpen) {
                switch (msg) {
                case WM_MOUSEMOVE:   case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP:   case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:   case WM_MOUSEWHEEL:
                    return 0;
                }
            }
        }
        return CallWindowProcW(g_wnd, h, msg, wp, lp);
    }

    // ========================================================================
    // RENDER (render thread)
    // ========================================================================
    void RenderFrame() {
        if (!g_imguiReady.load()) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = C.menuOpen;

        // ---- salva estado GL ----------------------------------------------
        const bool hasFbo = (p_glBindFramebuffer != nullptr);
        GLint     fbo = 0, vp[4] = { 0 }, sc[4] = { 0 };
        GLboolean hadScissor = glIsEnabled(GL_SCISSOR_TEST);
        GLboolean cmask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };

        if (hasFbo) glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetIntegerv(GL_SCISSOR_BOX, sc);
        glGetBooleanv(GL_COLOR_WRITEMASK, cmask);

        ImGuiIO& io = ImGui::GetIO();
        if (hasFbo) p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, (GLsizei)io.DisplaySize.x, (GLsizei)io.DisplaySize.y);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        // ---- desenho ------------------------------------------------------
        __try { ESP::Draw(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { Visuals::Draw(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { MenuDraw(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ---- restaura -----------------------------------------------------
        glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
        if (hasFbo) p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo);
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        glScissor(sc[0], sc[1], sc[2], sc[3]);
        if (hadScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }

    // ========================================================================
    // SWAP
    // ========================================================================
    static BOOL(WINAPI* o_wglSwap)(HDC) = nullptr;

    static BOOL WINAPI hk_wglSwap(HDC hdc) {
        g_inPresent.fetch_add(1);

        if (!g_imguiReady.load()) {
            g_hwnd = WindowFromDC(hdc);
            if (g_hwnd) {
                WNDPROC prev = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hkWnd);
                if (prev) g_wnd = prev;

                LoadGLExt();                       // contexto GL ja esta atual aqui

                ImGui::CreateContext();
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                ImGui::StyleColorsDark();
                ImGui_ImplWin32_Init(g_hwnd);
                ImGui_ImplOpenGL3_Init("#version 130");

                g_imguiReady.store(true);
                Log("imgui inicializado");
            }
        }

        __try { RenderFrame(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        __try { ForceCursorFree(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        BOOL r = o_wglSwap(hdc);
        g_inPresent.fetch_sub(1);
        return r;
    }

    // ========================================================================
    // INSTALL
    // ========================================================================
    bool InstallSwap() {
        if (MH_Initialize() != MH_OK) Log("MH_Initialize falhou");

        HMODULE gl = GetModuleHandleA("opengl32.dll");
        for (int i = 0; i < 100 && !gl; i++) { Sleep(100); gl = GetModuleHandleA("opengl32.dll"); }
        if (!gl) { Log("opengl32.dll NAO carregada"); return false; }

        void* p = (void*)GetProcAddress(gl, "wglSwapBuffers");
        if (!p) { Log("wglSwapBuffers NAO achado"); return false; }

        if (MH_CreateHook(p, hk_wglSwap, (LPVOID*)&o_wglSwap) != MH_OK) {
            Log("MH_CreateHook(swap) falhou");
            return false;
        }
        g_swapAddr = p;
        MH_EnableHook(p);
        Log("hook wglSwapBuffers OK");
        return true;
    }

    bool InstallMainThread() {
        const MethodInfo* target = SDK::M_LateUpdate;
        const char* who = "PlayerScript.LateUpdate";

        if (!target || !target->methodPointer) {
            static const char* classes[] = { "MainCamera", "SmoothCameraOrbit", "LeanTween", "GMVehicleLOD" };
            static const char* methods[] = { "LateUpdate", "Update" };
            target = nullptr;
            for (int ci = 0; ci < 4 && !target; ci++) {
                Il2CppClass* k = SDK::FindClass(classes[ci]);
                if (!k) continue;
                for (int mi = 0; mi < 2 && !target; mi++) {
                    const MethodInfo* m = IL2CPP::class_get_method_from_name(k, methods[mi], 0);
                    if (m && m->methodPointer) { target = m; who = classes[ci]; }
                }
            }
        }

        if (!target || !target->methodPointer) { Log("main-thread NAO resolvido"); return false; }
        Log("main-thread via %s", who);

        if (MH_CreateHook((LPVOID)target->methodPointer, hk_MainTick, (LPVOID*)&o_MainTick) != MH_OK) {
            Log("MH_CreateHook(main) falhou");
            return false;
        }
        MH_EnableHook((LPVOID)target->methodPointer);
        Log("hook main-thread OK");
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
            (SDK::M_set_lockState && SDK::M_set_lockState->methodPointer) ? "OK" : "-",
            (SDK::M_set_visible && SDK::M_set_visible->methodPointer) ? "OK" : "-");
        return ok;
    }

    bool InstallCombat() { return Combat::Install(); }

    // ========================================================================
    // SHUTDOWN
    // ========================================================================
    void Shutdown() {
        Log("shutdown...");
        g_hooksLive.store(false);

        for (int i = 0; i < 40 && g_inPresent.load() > 0; i++) Sleep(50);

        MH_DisableHook(MH_ALL_HOOKS);

        if (g_imguiReady.load()) {
            g_imguiReady.store(false);
            Sleep(100);
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        if (g_hwnd && g_wnd) SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_wnd);

        MH_Uninitialize();
        Log("shutdown ok");
    }

    void SaveConfig() { ConfigIO::Save(); }

} // namespace Hooks