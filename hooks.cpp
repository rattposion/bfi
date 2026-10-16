// ============================================================================
//  hooks.cpp
//
//  >>> FIX desta versao (o log provou):
//   1) FireOneShot NAO e chamado no cliente (zero linhas [fireOne]); fica como
//      no-op informativo.
//   2) O restore para camBefore era NO-OP: o kick cai ANTES do proximo tiro, e
//      o camBefore (capturado na entrada do FireBullet) ja vinha chutado.
//      preTiroCam(tiro N) == camTf chutado do tiro N-1.
//   3) A mira LIMPA e o playerRot: ele fica cravado durante o burst inteiro
//      enquanto o camTf acumula -0.0017/tiro. AGORA o camTf e forcado a seguir
//      o playerRot todo frame (fim do LateUpdate e no present).
//   4) Mantidos: FireBullet (silent aim + aim lock), probe/watch, ImGui blindado.
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

#define RECOIL_RESTORE_NEXT_FRAME 1
#define WATCH_MAX_LINES           60

// ---------------------------------------------------------------------------
//  Estado exposto pelo aimbot.cpp
// ---------------------------------------------------------------------------
extern bool g_silentAimActive;
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

    static std::atomic<bool> g_mainIsPlayerClass{ false };

    // ---- anti-recoil ----
    static Quat     g_pendingCam{ 0.f, 0.f, 0.f, 1.f };   // so p/ log (preTiroCam)
    static std::atomic<uint64_t> g_lastShotMs{ 0 };
    static int      g_watchLines = 0;

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

    static Il2CppClass* ClassOf(void* o) {
        if (!o || !il2cpp_object_get_class) return nullptr;
        return il2cpp_object_get_class((const Il2CppObject*)o);
    }

    // -----------------------------------------------------------------------
    //  NO-RECOIL — o camTf tem que seguir o playerRot (mira limpa)
    // -----------------------------------------------------------------------
    static void ApplyNoRecoil() {
        if (!C.noRecoil || !SDK::g_localPlayer) return;
        __try {
            Il2CppObject* ct = SDK::CamTfObj();
            if (!ct) return;
            Quat pr = SDK::GetPlayerRot(SDK::g_localPlayer);
            SDK::SetRot(ct, pr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
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

    static CRITICAL_SECTION  g_imCs;
    static bool              g_imCsInit = false;
    static std::atomic<bool> g_frameOpen{ false };

    struct ImLock {
        ImLock() { if (g_imCsInit) EnterCriticalSection(&g_imCs); }
        ~ImLock() { if (g_imCsInit) LeaveCriticalSection(&g_imCs); }
    };

    ImFont* SafeFont(int idx) {
        if (!g_imguiReady.load()) return nullptr;
        ImGuiIO& io = ImGui::GetIO();
        if (io.Fonts->Fonts.Size <= 0) return nullptr;
        if (idx < 0 || idx >= io.Fonts->Fonts.Size) idx = 0;
        return io.Fonts->Fonts[idx];
    }

    static LRESULT CALLBACK hk_wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (g_imguiInit) {
            ImLock lk;
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
        if (!g_imCsInit) { InitializeCriticalSection(&g_imCs); g_imCsInit = true; }
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        if (!ImGui_ImplWin32_Init(hwnd)) { Log("[hooks] ImGui_ImplWin32_Init FALHOU"); return; }
        if (!ImGui_ImplOpenGL3_Init("#version 130")) { Log("[hooks] ImGui_ImplOpenGL3_Init FALHOU"); return; }
        g_imguiInit = true;
        g_imguiReady = true;
        Log("[hooks] ImGui inicializado (hwnd=%p fontes=%d)", hwnd, io.Fonts->Fonts.Size);
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

        // ultima palavra do frame: se algo reaplicou o kick depois do LateUpdate,
        // zera aqui tambem.
        ApplyNoRecoil();

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
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return o_swap(hdc);
        g_viewW = w; g_viewH = h;

        {
            ImLock lk;

            if (g_frameOpen.exchange(true)) ImGui::EndFrame();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            Menu::Render();
            if (g_sdkReady.load()) { ESP::Draw(); Misc::Draw(); }

            ImGui::Render();
            g_frameOpen = false;

            ImDrawData* dd = ImGui::GetDrawData();
            if (dd && dd->Valid && dd->CmdListsCount > 0) {
                glViewport(0, 0, g_viewW, g_viewH);
                ImGui_ImplOpenGL3_RenderDrawData(dd);
            }
        }

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
    //  PROBE
    // -----------------------------------------------------------------------
    struct FloatProbe { uint32_t off; char name[40]; };
    static FloatProbe g_probe[512];
    static int        g_probeN = -1;
    static float      g_probeBefore[512];
    static int        g_probeShots = 0;
    static float      g_frameBefore[512];
    static int        g_frameProbeFrames = 0;

    static void CollectProbeFields(Il2CppClass* k) {
        g_probeN = 0;
        __try {
            for (Il2CppClass* c = k; c && g_probeN < 512; c = il2cpp_class_get_parent(c)) {
                void* it = nullptr;
                for (;;) {
                    FieldInfo* f = il2cpp_class_get_fields(c, &it);
                    if (!f || g_probeN >= 512) break;
                    const Il2CppType* t = il2cpp_field_get_type(f);
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                    if (!tn || !strstr(tn, "Single")) continue;
                    uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                    if (!off) continue;
                    g_probe[g_probeN].off = off;
                    strncpy_s(g_probe[g_probeN].name, il2cpp_field_get_name(f), _TRUNCATE);
                    ++g_probeN;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log("[probe] campos float em PlayerScript: %d", g_probeN);
    }

    static void ProbeSnapshot(float* dst) {
        for (int i = 0; i < g_probeN; ++i)
            dst[i] = SafeRead<float>((uintptr_t)SDK::g_localPlayer + g_probe[i].off, 0.f);
    }

    static void ProbeDiff(const float* ref, const char* tag) {
        if (!SDK::g_localPlayer || g_probeN <= 0) return;
        for (int i = 0; i < g_probeN; ++i) {
            float now = SafeRead<float>((uintptr_t)SDK::g_localPlayer + g_probe[i].off, 0.f);
            if (now != ref[i])
                Log("[%s] float %s @0x%X %.4f -> %.4f", tag, g_probe[i].name, g_probe[i].off, ref[i], now);
        }
    }

    // -----------------------------------------------------------------------
    //  main thread — SO chama Combat::Tick
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tLateUpdate)(void* self, void* method);
    static tLateUpdate o_lateUpdate = nullptr;

    static void __fastcall hk_lateUpdate(void* self, void* method) {
        PHASE("hk_lateUpdate");

        const bool isLocalInst = (!g_mainIsPlayerClass.load() ||
            !SDK::g_localPlayer || self == SDK::g_localPlayer);
        const bool watch = isLocalInst && g_lastShotMs.load() &&
            (GetTickCount64() - g_lastShotMs.load() < 150) &&
            g_frameProbeFrames < 20 && g_watchLines < WATCH_MAX_LINES;

        if (o_lateUpdate) o_lateUpdate(self, method);

        // >>> FIX: mata o recoil. O camTf TEM que seguir o playerRot (mira
        //     limpa). O kick deixa os dois diferentes e o camTf acumula.
        if (isLocalInst) ApplyNoRecoil();

        Quat r1{ 0.f, 0.f, 0.f, 1.f }, p1{ 0.f, 0.f, 0.f, 1.f };
        Il2CppObject* camTf = nullptr;
        if (watch) {
            if (g_probeN < 0) CollectProbeFields(ClassOf(SDK::g_localPlayer));
            camTf = SDK::CamTfObj();
            if (camTf) SDK::GetRot(camTf, r1);
            if (SDK::g_localPlayer) p1 = SDK::GetPlayerRot(SDK::g_localPlayer);

            ++g_frameProbeFrames;
            ++g_watchLines;

            Log("[watch] f%d camTf (%.4f,%.4f,%.4f,%.4f) playerRot (%.4f,%.4f,%.4f,%.4f) preTiroCam (%.4f,%.4f,%.4f,%.4f)",
                g_frameProbeFrames,
                r1.x, r1.y, r1.z, r1.w,
                p1.x, p1.y, p1.z, p1.w,
                g_pendingCam.x, g_pendingCam.y, g_pendingCam.z, g_pendingCam.w);

            ProbeSnapshot(g_frameBefore);
        }

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
    //  FIRE BULLET — (self, Int32, Single, Vector3, Vector3, Boolean, Double,
    //  Int32), retorna System.Boolean, sem MethodInfo*.
    // =======================================================================
    typedef bool(__fastcall* tFire7)(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        bool a5, double a6, int32_t a7);
    static tFire7 o_fire7 = nullptr;

    static std::atomic<uint32_t> g_fireCalls{ 0 };
    static int g_fireLogged = 0;

    static void __fastcall hk_fire7(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        bool a5, double a6, int32_t a7) {
        PHASE("hk_fire7");
        g_fireCalls.fetch_add(1, std::memory_order_relaxed);

        const bool isLocal = (SDK::g_localPlayer && self == SDK::g_localPlayer);
        Il2CppObject* camTf = isLocal ? SDK::CamTfObj() : nullptr;

        Quat camBefore{ 0.f, 0.f, 0.f, 1.f };
        bool haveRot = false;
        const bool aiming = (C.aimbot && C.aimKeyDown && SDK::g_aimTarget);

        __try {
            if (g_fireLogged < 3) {
                ++g_fireLogged;
                Log("[fire7] self=%p a1=%d(%p) a2=%.3f a3=(%.1f,%.1f,%.1f)|a3|=%.2f a4=(%.1f,%.1f,%.1f)|a4|=%.2f a5=%d a6=%.3f a7=%d",
                    self, a1, (void*)(intptr_t)a1, (double)a2,
                    a3.x, a3.y, a3.z, (double)a3.len(),
                    a4.x, a4.y, a4.z, (double)a4.len(),
                    (int)a5, a6, a7);
            }

            if (isLocal && camTf) {
                haveRot = SDK::GetRot(camTf, camBefore);

                // aim lock
                if (aiming && haveRot) {
                    Vec3 me = SDK::GetPos(camTf);
                    Vec3 tp = SDK::TargetAimPos(SDK::g_aimTarget, C.aimBone);
                    if (tp.zero()) tp = SDK::GetPos(SDK::g_aimTarget);
                    Vec3 d{ tp.x - me.x, tp.y - me.y, tp.z - me.z };
                    float L = d.len();
                    if (L > 0.0001f) {
                        Quat q = SDK::LookRot(d * (1.f / L));
                        SDK::SetRot(camTf, q);
                        SDK::SetPlayerRot(SDK::g_localPlayer, q);
                    }
                }

                if (C.noRecoil) g_pendingCam = camBefore;   // so p/ log
            }

            // silent aim / magic bullets
            if (isLocal && SDK::g_aimTarget && g_silentAimActive &&
                (C.silentAim || C.magicBullets)) {
                float d3 = fabsf(a3.len() - 1.f);
                float d4 = fabsf(a4.len() - 1.f);
                if ((d4 <= d3 ? d4 : d3) < 0.30f) {
                    int slot = (d4 <= d3) ? 1 : 0;
                    if (SDK::g_fireDirSlot.load() != slot) {
                        SDK::g_fireDirSlot.store(slot);
                        Log("[fire7] direcao = param%d", slot + 3);
                    }
                    if (slot == 1) a4 = g_silentAimDir; else a3 = g_silentAimDir;
                }
            }

            if (isLocal && g_probeShots < 6) {
                if (g_probeN < 0) CollectProbeFields(ClassOf(self));
                ProbeSnapshot(g_probeBefore);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[crash] hk_fire7 pre"); }

        bool ret = false;
        __try { ret = o_fire7 ? o_fire7(self, a1, a2, a3, a4, a5, a6, a7) : false; }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[crash] hk_fire7 original AV"); }

        // mata o kick se ele cair dentro do FireBullet (e reforca a mira limpa)
        if (isLocal) ApplyNoRecoil();

        if (isLocal) {
            g_lastShotMs.store(GetTickCount64());
            g_frameProbeFrames = 0;
            g_watchLines = 0;

            if (g_probeShots < 6 && camTf) {
                Quat r1{ 0.f, 0.f, 0.f, 1.f };
                SDK::GetRot(camTf, r1);
                Log("[probe] camTf rot (%.4f,%.4f,%.4f,%.4f) -> (%.4f,%.4f,%.4f,%.4f)",
                    camBefore.x, camBefore.y, camBefore.z, camBefore.w, r1.x, r1.y, r1.z, r1.w);
                ProbeDiff(g_probeBefore, "probe");
                Log("[probe] ---- fim diff tiro %d ----", ++g_probeShots);
            }
        }
    }

    // =======================================================================
    //  FIRE ONE SHOT — nao e chamado no cliente (no-op informativo)
    // =======================================================================
    typedef void(__fastcall* tFireOne)(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        uint8_t a5, uint8_t a6, double a7, int32_t a8, int64_t a9);
    static tFireOne o_fireOne = nullptr;
    static int      g_fireOneLogged = 0;

    static void __fastcall hk_fireOne(void* self, int32_t a1, float a2, Vec3 a3, Vec3 a4,
        uint8_t a5, uint8_t a6, double a7, int32_t a8, int64_t a9) {
        PHASE("hk_fireOne");

        if (g_fireOneLogged < 3) {
            ++g_fireOneLogged;
            Log("[fireOne] self=%p a1=%d(%p) a2=%.3f |a3|=%.2f |a4|=%.2f a5=%d a6=%d a7=%.2f a8=%d a9=%lld",
                self, a1, (void*)(intptr_t)a1, (double)a2,
                (double)a3.len(), (double)a4.len(), (int)a5, (int)a6, a7, a8, (long long)a9);
        }

        if (o_fireOne) o_fireOne(self, a1, a2, a3, a4, a5, a6, a7, a8, a9);

        ApplyNoRecoil();
    }

    // -----------------------------------------------------------------------
    //  shoot (nunca chamado, mantido)
    // -----------------------------------------------------------------------
    typedef void(__fastcall* tShoot)(void* self, void* method);
    static tShoot o_shoot1 = nullptr, o_shoot2 = nullptr, o_shoot3 = nullptr, o_shoot4 = nullptr;

    static void __fastcall hk_shoot1(void* s, void* m) { if (o_shoot1) o_shoot1(s, m); }
    static void __fastcall hk_shoot2(void* s, void* m) { if (o_shoot2) o_shoot2(s, m); }
    static void __fastcall hk_shoot3(void* s, void* m) { if (o_shoot3) o_shoot3(s, m); }
    static void __fastcall hk_shoot4(void* s, void* m) { if (o_shoot4) o_shoot4(s, m); }

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

            MethodInfo* fos = FindMethodByArity(ps, "FireOneShot", 9);
            if (fos) {
                void* p = SDK::MethodPtr(fos);
                Log("[hooks] FireOneShot: ret=%s ptr=%p", SDK::MethodReturnName(fos), p);
                if (p && MH_CreateHook(p, &hk_fireOne, (void**)&o_fireOne) == MH_OK) {
                    MH_EnableHook(p);
                    ok = true;
                    Log("[hooks] hook FireOneShot OK (anti-recoil)");
                }
            }
            else Log("[hooks] FireOneShot/9 nao encontrado");
        }
        else Log("[hooks] PlayerScript nulo");

        if (SDK::M_FireBullet) {
            uint32_t pc = il2cpp_method_get_param_count(SDK::M_FireBullet);
            void* p = SDK::MethodPtr(SDK::M_FireBullet);
            Log("[hooks] FireBullet: params=%u ret=%s ptr=%p", pc,
                SDK::MethodReturnName(SDK::M_FireBullet), p);
            if (p && pc == 7 && MH_CreateHook(p, &hk_fire7, (void**)&o_fire7) == MH_OK) {
                MH_EnableHook(p);
                ok = true;
                Log("[hooks] hook FireBullet OK (7 params, ret bool)");
            }
        }
        else Log("[hooks] M_FireBullet nulo");

        return ok;
    }

    // -----------------------------------------------------------------------
    void ApplyFeatures() {
        Log("[hooks] ApplyFeatures: noRecoil=%d noSpread=%d infAmmo=%d rapidFire=%d noReload=%d silent=%d",
            (int)C.noRecoil, (int)C.noSpread, (int)C.infAmmo, (int)C.rapidFire,
            (int)C.noReload, (int)C.silentAim);
    }

    bool Init() {
        if (g_mhInit) return true;
        if (MH_Initialize() != MH_OK) { Log("[hooks] MH_Initialize FALHOU"); return false; }
        g_mhInit = true;

        if (!g_imCsInit) { InitializeCriticalSection(&g_imCs); g_imCsInit = true; }

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
        SDK::g_recoilHookActive = false;
        g_frameOpen = false;

        if (o_wndProc && g_hwnd) {
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)o_wndProc);
            o_wndProc = nullptr;
        }
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_mhInit = false;
        g_imguiInit = false;
        Log("[hooks] Shutdown");
    }

} // namespace Hooks