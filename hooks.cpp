// ============================================================================
//  hooks.cpp — hooks + orquestracao de teclas.
//  hk_lateUpdate chama SO Combat::Tick (sem tick duplicado).
//
//  >>> FIX desta versao:
//   1) hk_fire7 SEM o parametro MethodInfo* (o IL2CPP OMITE esse parametro no
//      PlayerScript::FireBullet). Antes o "m" recebia 0,1,2,... e todos os
//      argumentos ficavam deslocados -> AV + bala nao spawnava (arma atirava
//      mas nao dava dano).
//   2) hook de recoil (ComputeShotRecoil) DESLIGADO por padrao (ABI incerta).
//      noRecoil segue funcionando pelo restore de rotacao do ShootBody.
//   3) corpo do hk_fire7 dentro de __try/__except pra nao derrubar o jogo.
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <cmath>
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

// >>> FIX: liga/desliga o hook de recoil (ABI incerta). 0 = desligado.
#define ENABLE_RECOIL_HOOK 0

// ---------------------------------------------------------------------------
//  Estado exposto pelo aimbot.cpp (usado pelo hk_fire7)
// ---------------------------------------------------------------------------
extern bool g_silentAimActive;
extern bool g_magicBulletsActive;
extern Vec3 g_silentAimDir;

namespace Hooks {

    std::atomic<bool> g_imguiReady{ false };
    std::atomic<bool> g_hooksLive{ false };
    std::atomic<bool> g_sdkReady{ false };
    int g_viewW = 1920;
    int g_viewH = 1080;

    static bool g_mhInit = false;
    static std::atomic<uint32_t> g_frameCounter{ 0 };
    static uint32_t g_lastTickFrame = 0xFFFFFFFF;

    // o metodo hookado e uma instancia de player? (usado pelo hk_lateUpdate)
    static std::atomic<bool> g_mainIsPlayerClass{ false };

    static bool KeyDown(int vk) { return vk ? (GetAsyncKeyState(vk) & 0x8000) != 0 : false; }

    static bool Edge(int vk) {
        static bool prev[256] = {};
        if (!vk) return false;
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
    //  teclas
    // -----------------------------------------------------------------------
    void HandleKeys() {
        if (Edge(C.menuKey)) C.menuOpen = !C.menuOpen;
        if (Edge(C.panicKey)) Misc::Panic();
        if (Edge(C.saveKey)) ConfigIO::Save();
        if (Edge(C.unloadKey)) { ConfigIO::Save(); g_unload = true; }
        if (C.aimKey) C.aimKeyDown = KeyDown(C.aimKey);
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
        g_imguiReady = true;
        Log("[hooks] ImGui inicializado (hwnd=%p)", hwnd);
    }

    static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
        PHASE("hk_swap");
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
                (int)C.menuOpen, (int)g_sdkReady.load(), SDK::g_targetCount, SDK::g_localPlayer);
        }

        HandleKeys();

        if (!g_imguiInit) return o_swap(hdc);

        RECT rc;
        GetClientRect(g_hwnd, &rc);
        g_viewW = (int)(rc.right - rc.left);
        g_viewH = (int)(rc.bottom - rc.top);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();
        if (g_sdkReady.load()) {
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
    //  main thread — SO chama Combat::Tick
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tLateUpdate)(void* self, void* method);
    static tLateUpdate o_lateUpdate = nullptr;

    static void __fastcall hk_lateUpdate(void* self, void* method) {
        PHASE("hk_lateUpdate");
        if (o_lateUpdate) o_lateUpdate(self, method);

        // >>> FIX: so filtra por instancia se o hook for numa classe de player.
        //     Se for singleton (GameManager/Game/HUD), self nunca == g_localPlayer
        //     e o Combat::Tick morria inteiro.
        if (g_mainIsPlayerClass.load() && SDK::g_localPlayer && self != SDK::g_localPlayer)
            return;

        uint32_t f = g_frameCounter.load(std::memory_order_relaxed);
        if (f == g_lastTickFrame) return;
        g_lastTickFrame = f;

        SDK::g_frameId = f;
        Combat::Tick();
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

                    bool isPlayer = (strcmp(classes[c], "PlayerScript") == 0 ||
                        strcmp(classes[c], "Entity") == 0 ||
                        strcmp(classes[c], "Player") == 0);
                    g_mainIsPlayerClass = isPlayer;

                    Log("[hooks] MAIN THREAD = %s::%s (%p) filtroPorInstancia=%d",
                        classes[c], methods[m], ptr, (int)isPlayer);
                    return true;
                }
            }
        }
        Log("[hooks] main thread: NENHUM alvo encontrado");
        return false;
    }

    // =======================================================================
    //  FIRE BULLET
    //  Assinatura NATIVA real (lida do metadata no log):
    //      (self, Int32, Single, Vector3, Vector3, Boolean, Double, Int32)
    //
    //  >>> FIX: NAO existe parametro MethodInfo* aqui.
    //  O IL2CPP OMITE o MethodInfo* quando o metodo nao usa esse parametro.
    //  Antes, com o MethodInfo* declarado, o "m" recebia 0,1,2,... e TODOS os
    //  argumentos ficavam deslocados um registrador -> o original dava AV e a
    //  bala nunca spawnava (arma atirava mas nao dava dano).
    // =======================================================================
    typedef void(__fastcall* tFire7)(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        bool a5, double a6, int32_t a7);
    static tFire7 o_fire7 = nullptr;

    static std::atomic<uint32_t> g_fireCalls{ 0 };
    static int g_fireLogged = 0;

    static void __fastcall hk_fire7(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        bool a5, double a6, int32_t a7) {
        PHASE("hk_fire7");
        g_fireCalls.fetch_add(1, std::memory_order_relaxed);

        __try {
            // loga os 3 primeiros tiros pra confirmar o layout dos parametros
            // (|a3| ou |a4| tem que ser ~1 = vetor de direcao)
            if (g_fireLogged < 3) {
                ++g_fireLogged;
                Log("[fire7] self=%p a1=%d a2=%.3f a3=(%.1f,%.1f,%.1f)|a3|=%.2f a4=(%.1f,%.1f,%.1f)|a4|=%.2f a5=%d a6=%.3f a7=%d",
                    self, a1, (double)a2,
                    a3.x, a3.y, a3.z, (double)a3.len(),
                    a4.x, a4.y, a4.z, (double)a4.len(),
                    (int)a5, a6, a7);
            }

            // silent aim / magic bullets: sobrescreve o parametro que for a
            // DIRECAO (o vetor unitario; o outro e a posicao de spawn)
            bool isLocal = SDK::g_localPlayer && self == SDK::g_localPlayer;
            if (isLocal && SDK::g_aimTarget && g_silentAimActive &&
                (C.silentAim || C.magicBullets)) {
                float d3 = fabsf(a3.len() - 1.f);
                float d4 = fabsf(a4.len() - 1.f);
                Vec3* dir = (d4 <= d3) ? &a4 : &a3;
                if ((d4 <= d3 ? d4 : d3) < 0.30f) {
                    *dir = g_silentAimDir;
                    static uint32_t n = 0;
                    if ((n++ % 25u) == 0)
                        Log("[fire7] dir sobrescrita self=%p |a3|=%.2f |a4|=%.2f dir=%.2f,%.2f,%.2f",
                            self, (double)a3.len(), (double)a4.len(),
                            g_silentAimDir.x, g_silentAimDir.y, g_silentAimDir.z);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[crash] hk_fire7 pegou AV");
        }

        if (o_fire7) o_fire7(self, a1, a2, a3, a4, a5, a6, a7);

        static ULONGLONG lastLog = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastLog > 1000) {
            lastLog = now;
            Log("[fire7] chamadas=%u isLocal=%d alvo=%p silent=%d magic=%d",
                g_fireCalls.load(), (int)(SDK::g_localPlayer && self == SDK::g_localPlayer),
                SDK::g_aimTarget, (int)C.silentAim, (int)C.magicBullets);
        }
    }

    // -----------------------------------------------------------------------
    //  hooks de tiro 0-param (fallback, com rotacao da camera)
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tShoot)(void* self, const MethodInfo* m);

    static std::atomic<uint32_t> g_shootCalls{ 0 };

    static tShoot o_shoot1 = nullptr;
    static tShoot o_shoot2 = nullptr;
    static tShoot o_shoot3 = nullptr;
    static tShoot o_shoot4 = nullptr;

    static void ShootBody(void* self, const MethodInfo* m, tShoot orig) {
        PHASE("hk_shoot");
        g_shootCalls.fetch_add(1, std::memory_order_relaxed);

        bool isLocal = (!SDK::g_localPlayer || self == SDK::g_localPlayer);

        Quat original{};
        bool has = false;
        if (isLocal && C.aimbot && C.aimKeyDown && SDK::g_aimTarget && SDK::g_localPlayer) {
            if (SDK::AimLock(SDK::g_aimTarget, original)) has = true;
        }

        if (orig) orig(self, m);

        // magic bullets: atira no alvo (quando o FireBullet direto nao esta hookado)
        if (isLocal && C.magicBullets && SDK::g_aimTarget && SDK::FireBulletReady()) {
            Vec3 tp = SDK::TargetAimPos(SDK::g_aimTarget, C.aimBone);
            if (!tp.zero()) SDK::FireBulletAt(tp);
        }

        if (has && SDK::g_localPlayer) SDK::RestoreAim(SDK::g_localPlayer, original);

        static ULONGLONG lastLog = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastLog > 1000) {
            lastLog = now;
            Log("[shoot] chamadas=%u isLocal=%d aimTarget=%p keyDown=%d silent=%d magic=%d",
                g_shootCalls.load(), (int)isLocal, SDK::g_aimTarget,
                (int)C.aimKeyDown, (int)C.silentAim, (int)C.magicBullets);
        }
    }

    static void __fastcall hk_shoot1(void* s, const MethodInfo* m) { ShootBody(s, m, o_shoot1); }
    static void __fastcall hk_shoot2(void* s, const MethodInfo* m) { ShootBody(s, m, o_shoot2); }
    static void __fastcall hk_shoot3(void* s, const MethodInfo* m) { ShootBody(s, m, o_shoot3); }
    static void __fastcall hk_shoot4(void* s, const MethodInfo* m) { ShootBody(s, m, o_shoot4); }

    // -----------------------------------------------------------------------
    //  recoil (DESLIGADO por padrao — ABI incerta; ver #define no topo)
    // -----------------------------------------------------------------------
    typedef Quat(__fastcall* tRecoilF)(void*, const MethodInfo*, float, float, float, float);
    typedef Quat(__fastcall* tRecoilP)(void*, const MethodInfo*, void*, void*, void*, void*);
    static tRecoilF o_recoilF = nullptr;
    static tRecoilP o_recoilP = nullptr;

    static Quat __fastcall hk_recoil_f(void* self, const MethodInfo* m,
        float a1, float a2, float a3, float a4) {
        if (C.noRecoil) return Quat{ 0.f, 0.f, 0.f, 1.f };
        return o_recoilF ? o_recoilF(self, m, a1, a2, a3, a4) : Quat{ 0.f, 0.f, 0.f, 1.f };
    }

    static Quat __fastcall hk_recoil_p(void* self, const MethodInfo* m,
        void* a1, void* a2, void* a3, void* a4) {
        if (C.noRecoil) return Quat{ 0.f, 0.f, 0.f, 1.f };
        return o_recoilP ? o_recoilP(self, m, a1, a2, a3, a4) : Quat{ 0.f, 0.f, 0.f, 1.f };
    }

    static bool AllParamsFloat(const MethodInfo* m) {
        uint32_t pc = il2cpp_method_get_param_count(m);
        if (pc == 0) return false;
        for (uint32_t i = 0; i < pc && i < 4; ++i) {
            const Il2CppType* t = il2cpp_method_get_param(m, i);
            if (!t) return false;
            const char* tn = il2cpp_type_get_name(t);
            if (!tn || !strstr(tn, "Single")) return false;
        }
        return true;
    }

    bool InstallCombat() {
        PHASE("InstallCombat");
        bool ok = false;

        Il2CppClass* ps = SDK::FindClass("PlayerScript");
        if (ps) {
            static const char* names[] = { "SemiAutoShoot", "Shoot", "AutoShoot", "FullAutoShoot" };
            static void* detours[] = { (void*)&hk_shoot1, (void*)&hk_shoot2,
                                       (void*)&hk_shoot3, (void*)&hk_shoot4 };
            static tShoot* tramps[] = { &o_shoot1, &o_shoot2, &o_shoot3, &o_shoot4 };

            int n = 0;
            for (size_t i = 0; i < 4; ++i) {
                MethodInfo* m = FindMethodByArity(ps, names[i], 0);
                if (!m) { Log("[hooks] %s nao encontrado", names[i]); continue; }
                void* p = SDK::MethodPtr(m);
                if (!p) { Log("[hooks] %s sem ptr", names[i]); continue; }
                if (MH_CreateHook(p, detours[n], (void**)tramps[n]) == MH_OK) {
                    MH_EnableHook(p);
                    Log("[hooks] hook shoot OK: %s (%p)", names[i], p);
                    if (!SDK::M_shoot) SDK::M_shoot = m;
                    ok = true;
                    if (++n >= 4) break;
                }
            }
        }
        else Log("[hooks] PlayerScript nulo");

        // ---- FireBullet (silent aim / magic bullets) ----
        if (SDK::M_FireBullet) {
            uint32_t pc = il2cpp_method_get_param_count(SDK::M_FireBullet);
            void* p = SDK::MethodPtr(SDK::M_FireBullet);
            Log("[hooks] FireBullet: params=%u ptr=%p", pc, p);
            if (p && pc == 7 && MH_CreateHook(p, &hk_fire7, (void**)&o_fire7) == MH_OK) {
                MH_EnableHook(p);
                ok = true;
                Log("[hooks] hook FireBullet OK (7 params, SEM MethodInfo)");
            }
        }
        else Log("[hooks] M_FireBullet nulo");

        // ---- recoil (informativo + hook opcional) ----
        if (!SDK::M_recoil && ps) {
            SDK::M_recoil = FindMethodAny(ps, "ComputeShotRecoil");
            if (!SDK::M_recoil) SDK::M_recoil = FindMethodAny(ps, "ShotRecoil");
        }

        if (SDK::M_recoil) {
            uint32_t pc = il2cpp_method_get_param_count(SDK::M_recoil);
            void* p = SDK::MethodPtr(SDK::M_recoil);
            Log("[hooks] recoil: params=%u ptr=%p allFloat=%d hook=%d",
                pc, p, (int)AllParamsFloat(SDK::M_recoil), (int)ENABLE_RECOIL_HOOK);

#if ENABLE_RECOIL_HOOK
            if (p && AllParamsFloat(SDK::M_recoil) && pc > 0 && pc <= 4) {
                if (MH_CreateHook(p, &hk_recoil_f, (void**)&o_recoilF) == MH_OK) {
                    MH_EnableHook(p); ok = true; Log("[hooks] hook recoil OK (float)");
                }
            }
            else if (p && MH_CreateHook(p, &hk_recoil_p, (void**)&o_recoilP) == MH_OK) {
                MH_EnableHook(p); ok = true; Log("[hooks] hook recoil OK (ptr/0arg)");
            }
#else
            (void)p; (void)pc;
#endif
        }
        else Log("[hooks] M_recoil nulo");

        return ok;
    }

    // -----------------------------------------------------------------------
    void ApplyFeatures() {
        Log("[hooks] ApplyFeatures: noRecoil=%d noSpread=%d infAmmo=%d speed=%d antiCheat=%d",
            (int)C.noRecoil, (int)C.noSpread, (int)C.infAmmo, (int)C.speedHack, (int)C.antiCheat);
    }

    bool Init() {
        if (g_mhInit) return true;
        if (MH_Initialize() != MH_OK) { Log("[hooks] MH_Initialize FALHOU"); return false; }
        g_mhInit = true;

        InstallSwap();
        InstallMainThread();
        InstallCursor();
        InstallCombat();

        Aimbot::Install();
        Triggerbot::Install();
        AntiCheat::Install();

        ApplyFeatures();

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