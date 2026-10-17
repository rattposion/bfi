// ============================================================================
//  weapons.cpp — stats + ANTI-RECOIL por neutralizacao dos campos de origem
//
//  >>> O arquivo inteiro esta dentro de #ifndef BFI_WEAPONS_CPP. Se ele for
//      colado duas vezes no mesmo arquivo, a 2a copia e PULADA em silencio
//      (antes era um #error que so reclamava). Tambem protege contra
//      #include duplicado.
//
//  >>> FIX FPS: o Tick resolvia o objeto de gunStats TODO FRAME
//      (FindGunStatsObj varre campos + array de loadout + get_activeInHierarchy).
//      Agora a RESOLUCAO e' cacheada por 250 ms; as ESCRITAS continuam por frame.
// ============================================================================
#ifndef BFI_WEAPONS_CPP
#define BFI_WEAPONS_CPP

#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"
#include "il2cpp_api.h"

#include <windows.h>
#include <cstring>
#include <vector>

// marcador de build. selectany = merge entre TUs (evita LNK2005 se incluido).
extern "C" __declspec(selectany) int bfi_weapons_obj = 0xBF;

namespace Weapons {

    static uint32_t o_cur = 0, o_max = 0, o_reserve = 0;
    static uint32_t o_reload = 0, o_spread = 0, o_rate = 0, o_dmg = 0;
    static Il2CppObject* g_stats = nullptr;
    static bool g_dumped = false;
    static bool g_ammoDumped = false;

    // ------------------------- anti-recoil -------------------------
    //  MODO_ZERO   = campo de KICK aditivo       -> zera (0.0)
    //  MODO_NEUTRO = campo MULTIPLICADOR/escala  -> volta ao neutro (1.0)
    enum { MODO_ZERO = 0, MODO_NEUTRO = 1 };

    struct RecField { uint32_t off; int kind; int modo; char name[48]; };

    static std::vector<RecField> g_gunRec, g_viewRec, g_plRec;
    static bool g_gunRecDone = false, g_viewRecDone = false, g_plRecDone = false;

    static ULONGLONG g_lastRecLog = 0;
    static int       g_recLogs = 0;

    static void Lower(char* dst, size_t n, const char* src) {
        size_t i = 0;
        if (!dst || !n) return;
        if (!src) { dst[0] = 0; return; }
        for (; src[i] && i + 1 < n; ++i) {
            char c = src[i];
            dst[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        dst[i] = 0;
    }

    static bool NameHasRecoil(const char* n) {
        char l[64];
        Lower(l, sizeof(l), n);
        if (!strstr(l, "recoil") && !strstr(l, "kick") &&
            !strstr(l, "punch") && !strstr(l, "climb")) return false;
        if (strstr(l, "speed") || strstr(l, "recovery") || strstr(l, "lerp") ||
            strstr(l, "smooth") || strstr(l, "time") || strstr(l, "rate")) return false;
        return true;
    }

    // campo tipo "recoilStability" e MULTIPLICADOR (normalmente 1.0-1.5).
    // Zerar ele deixava a estabilidade em 0 -> agora vai pro neutro.
    static int ModeForName(const char* n) {
        char l[64];
        Lower(l, sizeof(l), n);
        if (strstr(l, "stability") || strstr(l, "scale") || strstr(l, "factor") ||
            strstr(l, "mult") || strstr(l, "modifier") || strstr(l, "strength") ||
            strstr(l, "damp") || strstr(l, "resist")) return MODO_NEUTRO;
        return MODO_ZERO;
    }

    static void CollectRec(Il2CppClass* k, std::vector<RecField>& out,
        const char* tag, bool& done) {
        if (done) return;
        done = true;
        out.clear();
        if (!k) { Log("[recoil] %s: classe nula", tag); return; }

        __try {
            for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
                void* it = nullptr;
                for (;;) {
                    FieldInfo* f = il2cpp_class_get_fields(c, &it);
                    if (!f) break;

                    const char* n = il2cpp_field_get_name(f);
                    if (!NameHasRecoil(n)) continue;

                    const Il2CppType* t = il2cpp_field_get_type(f);
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;

                    int kind = -1;
                    if (tn) {
                        if (strstr(tn, "Single")) kind = 0;
                        else if (strstr(tn, "Vector2")) kind = 1;
                        else if (strstr(tn, "Vector3")) kind = 2;
                    }
                    if (kind < 0) continue;

                    uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                    if (!off) continue;

                    RecField rf{};
                    rf.off = off;
                    rf.kind = kind;
                    rf.modo = ModeForName(n);
                    strncpy_s(rf.name, n, _TRUNCATE);
                    out.push_back(rf);

                    Log("[recoil] %s: %s @0x%X (%s, modo=%s)", tag, n, off,
                        kind == 0 ? "float" : (kind == 1 ? "vec2" : "vec3"),
                        rf.modo == MODO_NEUTRO ? "NEUTRO/1.0" : "ZERO/0.0");
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[recoil] %s: excecao na varredura", tag);
        }

        Log("[recoil] %s: %d campos casados", tag, (int)out.size());
    }

    static void LogRecValues(Il2CppObject* o, const std::vector<RecField>& v, const char* tag) {
        if (!o) return;
        for (size_t i = 0; i < v.size(); ++i) {
            uintptr_t a = (uintptr_t)o + v[i].off;
            if (v[i].kind == 0) {
                Log("[recoil] %s valor %s = %.4f", tag, v[i].name, SafeRead<float>(a, 0.f));
            }
            else if (v[i].kind == 1) {
                Log("[recoil] %s valor %s = (%.3f, %.3f)", tag, v[i].name,
                    SafeRead<float>(a, 0.f), SafeRead<float>(a + 4, 0.f));
            }
            else {
                Log("[recoil] %s valor %s = (%.3f, %.3f, %.3f)", tag, v[i].name,
                    SafeRead<float>(a, 0.f), SafeRead<float>(a + 4, 0.f),
                    SafeRead<float>(a + 8, 0.f));
            }
        }
    }

    static void NeutralizeAll(Il2CppObject* o, const std::vector<RecField>& v) {
        if (!o) return;
        for (size_t i = 0; i < v.size(); ++i) {
            uintptr_t a = (uintptr_t)o + v[i].off;
            float val = (v[i].modo == MODO_NEUTRO) ? 1.f : 0.f;
            if (v[i].kind == 0) {
                SafeWrite(a, (const void*)&val, sizeof(float));
            }
            else if (v[i].kind == 1) {
                float z[2] = { val, val };
                SafeWrite(a, (const void*)z, sizeof(z));
            }
            else {
                float z[3] = { val, val, val };
                SafeWrite(a, (const void*)z, sizeof(z));
            }
        }
    }

    static void DumpAmmoFields(Il2CppClass* k) {
        if (!k) return;
        Log("[ammo] ---- campos de municao em %s ----",
            (il2cpp_class_get_name ? il2cpp_class_get_name(k) : "?"));

        __try {
            for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
                void* it = nullptr;
                for (;;) {
                    FieldInfo* f = il2cpp_class_get_fields(c, &it);
                    if (!f) break;

                    const char* n = il2cpp_field_get_name(f);
                    if (!n) continue;

                    char l[64];
                    Lower(l, sizeof(l), n);
                    if (!strstr(l, "ammo") && !strstr(l, "mag") &&
                        !strstr(l, "clip") && !strstr(l, "bullet") &&
                        !strstr(l, "round")) continue;

                    const Il2CppType* t = il2cpp_field_get_type(f);
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : "?";

                    Log("[ammo]   %-30s : %-16s @0x%X", n, tn ? tn : "?",
                        (unsigned)il2cpp_field_get_offset(f));
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[ammo] excecao na varredura"); }

        Log("[ammo] ---- fim ----");
    }

    static void DumpGunArray(const char* tag, void* arr) {
        if (!arr) return;
        __try {
            int len = 0;
            if (il2cpp_array_length) len = (int)il2cpp_array_length((const Il2CppArray*)arr);
            if (len <= 0) len = *(int*)((uintptr_t)arr + 0x18);   // 0x18 = max_length

            Log("[guns] %s: len=%d", tag, len);
            if (len < 0 || len > 64) len = 64;
            for (int i = 0; i < len; ++i) {
                void* e = *(void**)((uintptr_t)arr + 0x20 + (size_t)i * 8);
                if (!e) { Log("[guns]   [%d] null", i); continue; }
                Il2CppClass* k = il2cpp_object_get_class((Il2CppObject*)e);
                const char* kn = k ? il2cpp_class_get_name(k) : "?";
                Log("[guns]   [%d] %s @%p | RPM=%.1f magSize=%d ammo=%d gunObj=%p",
                    i, kn, e,
                    SafeRead<float>((uintptr_t)e + 0x48, -1.f),
                    SafeRead<int>((uintptr_t)e + 0xFC, -1),
                    SafeRead<int>((uintptr_t)e + 0x90, -1),
                    SDK::SafeReadObjPub(e, 0x148));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[guns] %s: excecao", tag); }
    }

    static void DumpGunCandidates() {
        Il2CppObject* lp = SDK::g_localPlayer;
        if (!lp) return;
        Il2CppClass* k = il2cpp_object_get_class(lp);
        if (!k) return;

        Log("[guns] O_gunStats=0x%X  cls=%s", SDK::O_gunStats, il2cpp_class_get_name(k));
        Log("[guns] ---- campos tipo Gun/Weapon em %s ----", il2cpp_class_get_name(k));

        __try {
            for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
                void* it = nullptr;
                for (;;) {
                    FieldInfo* f = il2cpp_class_get_fields(c, &it);
                    if (!f) break;

                    const char* fn = il2cpp_field_get_name(f);
                    const Il2CppType* t = il2cpp_field_get_type(f);
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                    if (!tn) continue;
                    if (!strstr(tn, "Gun") && !strstr(tn, "gun") &&
                        !strstr(tn, "Weapon") && !strstr(tn, "weapon")) continue;

                    uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                    if (!off) continue;

                    void* p = SDK::SafeReadObjPub(lp, off);
                    if (!p) {
                        Log("[guns]   %-26s : %-30s @0x%X = null", fn, tn, off);
                        continue;
                    }

                    Il2CppClass* ok = il2cpp_object_get_class((Il2CppObject*)p);
                    const char* ocn = ok ? il2cpp_class_get_name(ok) : "?";

                    if (ocn && (strstr(ocn, "gunStats") || strstr(ocn, "GunStats"))) {
                        Log("[guns]   %-26s : %s @0x%X -> %s @%p | RPM=%.1f magSize=%d ammo=%d gunObj=%p",
                            fn, tn, off, ocn, p,
                            SafeRead<float>((uintptr_t)p + 0x48, -1.f),
                            SafeRead<int>((uintptr_t)p + 0xFC, -1),
                            SafeRead<int>((uintptr_t)p + 0x90, -1),
                            SDK::SafeReadObjPub(p, 0x148));
                        if (strstr(tn, "[]")) DumpGunArray(fn, p);
                    }
                    else {
                        Log("[guns]   %-26s : %s @0x%X -> %s @%p", fn, tn, off, ocn, p);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[guns] excecao na varredura"); }

        if (SDK::O_gunView) {
            Il2CppObject* view = SDK::SafeReadObjPub(lp, SDK::O_gunView);
            if (view) {
                Log("[guns] ---- fields da fpsGunView ----");
                SDK::DumpFieldsWithValues(view);
            }
        }

        Log("[guns] ---- fim ----");
    }

    // ------------------------- tick -------------------------
    void Tick() {
        if (!(C.infAmmo || C.noReload || C.noSpread || C.rapidFire || C.noRecoil)) return;
        if (!SDK::g_localPlayer) return;

        // >>> FIX FPS: FindGunStatsObj varre campos + array de loadout e chama
        //     get_activeInHierarchy. Resolver 1x a cada 250 ms basta — as
        //     ESCRITAS continuam por frame, so a RESOLUCAO e' cacheada.
        static Il2CppObject* s_stats = nullptr;
        static ULONGLONG     s_statsNext = 0;
        ULONGLONG nowStats = GetTickCount64();
        if (nowStats >= s_statsNext) {
            s_statsNext = nowStats + 250;
            s_stats = SDK::FindGunStatsObj();
        }
        Il2CppObject* stats = s_stats;
        if (stats) {
            if (stats != g_stats) {
                g_stats = stats;

                SDK::ResolveWeaponFields(stats,
                    o_cur, o_max, o_reserve, o_reload, o_spread, o_rate, o_dmg);

                if (!g_dumped) { g_dumped = true; SDK::DumpFieldsWithValues(stats); }

                if (!g_ammoDumped) {
                    g_ammoDumped = true;
                    DumpAmmoFields(il2cpp_object_get_class(stats));
                    DumpGunCandidates();
                }

                g_gunRecDone = false;
                CollectRec(il2cpp_object_get_class(stats), g_gunRec, "gun", g_gunRecDone);
                LogRecValues(stats, g_gunRec, "gun");

                g_recLogs = 0;
                g_lastRecLog = 0;
            }

            if (C.infAmmo) {
                if (o_cur) {
                    int mx = o_max ? SafeRead<int>((uintptr_t)stats + o_max, 0) : 0;
                    SafeWrite((uintptr_t)stats + o_cur, (mx > 0 && mx < 10000) ? mx : 999);
                }
                if (o_reserve) SafeWrite((uintptr_t)stats + o_reserve, 999);
            }

            if (C.noReload && o_reload) SafeWrite<bool>((uintptr_t)stats + o_reload, false);

            if (C.noSpread && o_spread) SafeWrite((uintptr_t)stats + o_spread, 0.f);

            if (C.rapidFire && o_rate) {
                if (SDK::g_rateIsRpm) {
                    SafeWrite((uintptr_t)stats + o_rate, 3000.f);
                }
                else {
                    float cur = SafeRead<float>((uintptr_t)stats + o_rate, 0.f);
                    SafeWrite((uintptr_t)stats + o_rate, (cur >= 1.f) ? 100.f : 0.01f);
                }
            }

            if (C.noRecoil) {
                NeutralizeAll(stats, g_gunRec);

                ULONGLONG now = GetTickCount64();
                if (g_recLogs < 8 && now - g_lastRecLog > 2000) {
                    g_lastRecLog = now;
                    ++g_recLogs;
                    LogRecValues(stats, g_gunRec, "gun*");
                }
            }
        }

        if (C.noRecoil) {
            if (!g_plRecDone) {
                CollectRec(il2cpp_object_get_class(SDK::g_localPlayer), g_plRec,
                    "player", g_plRecDone);
                LogRecValues(SDK::g_localPlayer, g_plRec, "player");
            }
            NeutralizeAll(SDK::g_localPlayer, g_plRec);

            if (SDK::O_gunView) {
                Il2CppObject* view = SDK::SafeReadObjPub(SDK::g_localPlayer, SDK::O_gunView);
                if (view) {
                    if (!g_viewRecDone) {
                        CollectRec(il2cpp_object_get_class(view), g_viewRec,
                            "gunview", g_viewRecDone);
                        LogRecValues(view, g_viewRec, "gunview");
                    }
                    NeutralizeAll(view, g_viewRec);
                }
            }
        }
    }

} // namespace Weapons

#endif // BFI_WEAPONS_CPP