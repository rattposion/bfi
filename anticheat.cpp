// ============================================================================
//  anticheat.cpp — bypass em camadas
//
//  >>> FIX desta versao (C3861 / C2027):
//   1) il2cpp_field_get_flags e il2cpp_field_static_set_value NAO estao no
//      il2cpp_api.h do projeto -> resolvidas direto do export da
//      GameAssembly.dll, com nomes locais (g_fieldGetFlags / g_fieldStaticSet)
//      pra nao colidir com um extern do header.
//   2) Il2CppClass e tipo INCOMPLETO -> nao da pra usar c->name. Trocado pelo
//      getter il2cpp_class_get_name (tambem resolvido local: g_classGetName).
//
//  Camadas do bypass:
//   1) Descobre a classe do anti-cheat e o objeto vivo (estatico -> get_Instance
//      -> varredura dos campos do g_localPlayer).
//   2) Forca todo getter "flag" (bool, 0 params) a retornar FALSE.
//   3) Zera flags bool e contadores int, antes E depois do Update do AC.
//   4) Reancora campos "ultima posicao" na posicao atual.
//   5) Limpa tambem os statics da classe.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include "MinHook.h"

#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <vector>

void Log(const char* fmt, ...);

// ---------------------------------------------------------------------------
//  API extra do il2cpp (nao declarada no il2cpp_api.h do projeto).
//  Nomes locais distintos -> zero risco de conflito de simbolo.
// ---------------------------------------------------------------------------
namespace {

    typedef uint32_t(*t_field_get_flags)(FieldInfo*);
    typedef void        (*t_field_static_set_value)(FieldInfo*, void*);
    typedef const char* (*t_class_get_name)(Il2CppClass*);

    static t_field_get_flags        g_fieldGetFlags = nullptr;
    static t_field_static_set_value g_fieldStaticSet = nullptr;
    static t_class_get_name         g_classGetName = nullptr;

    static void ResolveAcApi() {
        static bool done = false;
        if (done) return;
        done = true;

        HMODULE h = GetModuleHandleA("GameAssembly.dll");
        if (!h) { Log("[ac] GameAssembly.dll nao carregada — api extra indisponivel"); return; }

        g_fieldGetFlags = (t_field_get_flags)GetProcAddress(h, "il2cpp_field_get_flags");
        g_fieldStaticSet = (t_field_static_set_value)GetProcAddress(h, "il2cpp_field_static_set_value");
        g_classGetName = (t_class_get_name)GetProcAddress(h, "il2cpp_class_get_name");

        Log("[ac] api extra: get_flags=%d static_set=%d class_name=%d",
            (int)(g_fieldGetFlags != nullptr),
            (int)(g_fieldStaticSet != nullptr),
            (int)(g_classGetName != nullptr));
    }

    // FIELD_ATTRIBUTE_STATIC = 0x10
    static bool IsStaticField(FieldInfo* f) {
        return (g_fieldGetFlags && f) ? ((g_fieldGetFlags(f) & 0x10u) != 0u) : false;
    }

    static const char* ClassName(Il2CppClass* k) {
        return (g_classGetName && k) ? g_classGetName(k) : "?";
    }

    static void StaticSet(FieldInfo* f, void* v) {
        if (g_fieldStaticSet && f) g_fieldStaticSet(f, v);
    }

} // namespace

namespace AntiCheat {

    // ---------------------------------------------------------------- estado
    static Il2CppClass* g_cls = nullptr;
    static Il2CppObject* g_obj = nullptr;

    struct FieldRef { uint32_t off; char name[48]; };
    static std::vector<FieldRef> g_bools;
    static std::vector<FieldRef> g_ints;
    static std::vector<FieldRef> g_vecs;

    struct StaticRef { FieldInfo* f; char name[48]; bool isBool; };
    static std::vector<StaticRef> g_statics;

    static void* o_acUpdate = nullptr;
    static uint64_t g_lastTry = 0;
    static uint32_t g_ticks = 0;
    static uint32_t g_cleared = 0;

    // ---------------------------------------------------------------- helpers
    static uint32_t FieldOff(Il2CppClass* k, const char* name) {
        if (!k) return 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            FieldInfo* f = il2cpp_class_get_field_from_name(c, name);
            if (f) return (uint32_t)il2cpp_field_get_offset(f);
        }
        return 0;
    }

    static uint32_t Pick(Il2CppClass* k, std::initializer_list<const char*> names) {
        for (const char* n : names) {
            uint32_t o = FieldOff(k, n);
            if (o) return o;
        }
        return 0;
    }

    static void Lower(const char* s, char* out, size_t n) {
        size_t i = 0;
        for (; s && s[i] && i + 1 < n; ++i) {
            char c = s[i];
            out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        out[i] = 0;
    }

    static bool NameHas(const char* low, std::initializer_list<const char*> keys) {
        for (const char* k : keys) if (strstr(low, k)) return true;
        return false;
    }

    // 0=outro 1=bool 2=int 3=float 4=vec3
    static int TypeKind(FieldInfo* f) {
        if (!f) return 0;
        const Il2CppType* t = il2cpp_field_get_type(f);
        const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
        if (!tn) return 0;
        if (strstr(tn, "Boolean")) return 1;
        if (strstr(tn, "Int32"))   return 2;
        if (strstr(tn, "Single"))  return 3;
        if (strstr(tn, "Vector3")) return 4;
        return 0;
    }

    static void ScanFields(Il2CppClass* k, const char* tag) {
        if (!k) return;
        int nb = 0, ni = 0, nv = 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStaticField(f)) continue;          // static -> ScanStatics
                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (!off) continue;
                const char* fn = il2cpp_field_get_name(f);
                if (!fn) continue;

                char low[96]; Lower(fn, low, sizeof(low));
                int kind = TypeKind(f);

                if (kind == 1) {
                    if (NameHas(low, { "block", "speed", "fly", "noclip", "cheat",
                                       "hack", "detect", "violation", "warn",
                                       "kick", "ban", "suspect", "teleport",
                                       "desync", "lag", "afk", "idle", "freeze" })) {
                        FieldRef r{}; r.off = off;
                        strncpy_s(r.name, fn, _TRUNCATE);
                        g_bools.push_back(r); ++nb;
                    }
                }
                else if (kind == 2) {
                    if (NameHas(low, { "violation", "warn", "strike", "count", "cheat",
                                       "hack", "detect", "kick", "stage", "level",
                                       "block", "speed", "fly", "noclip" })) {
                        FieldRef r{}; r.off = off;
                        strncpy_s(r.name, fn, _TRUNCATE);
                        g_ints.push_back(r); ++ni;
                    }
                }
                else if (kind == 4) {
                    if (NameHas(low, { "last", "prev", "old", "start", "check", "base" })) {
                        FieldRef r{}; r.off = off;
                        strncpy_s(r.name, fn, _TRUNCATE);
                        g_vecs.push_back(r); ++nv;
                    }
                }
            }
        }
        Log("[ac] %s: bools=%d ints=%d vecs=%d", tag, nb, ni, nv);
    }

    static void ScanStatics(Il2CppClass* k) {
        if (!k) return;
        int n = 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (!IsStaticField(f)) continue;         // so static
                int kind = TypeKind(f);
                if (kind != 1 && kind != 2) continue;
                const char* fn = il2cpp_field_get_name(f);
                if (!fn) continue;
                char low[96]; Lower(fn, low, sizeof(low));
                if (!NameHas(low, { "block", "speed", "fly", "noclip", "cheat", "hack",
                                    "detect", "violation", "warn", "kick", "ban", "suspect" }))
                    continue;
                StaticRef r{}; r.f = f; r.isBool = (kind == 1);
                strncpy_s(r.name, fn, _TRUNCATE);
                g_statics.push_back(r); ++n;
            }
        }
        Log("[ac] statics de flag: %d", n);
    }

    // ---------------------------------------------------------------- getters
#define AC_SLOT(N)                                                        \
        static void* o_acm##N = nullptr;                                      \
        static bool __fastcall hk_acm##N(void* self) {                        \
            if (o_acm##N) ((bool(__fastcall*)(void*))o_acm##N)(self);         \
            return false;                                                     \
        }
    AC_SLOT(0)  AC_SLOT(1)  AC_SLOT(2)  AC_SLOT(3)
        AC_SLOT(4)  AC_SLOT(5)  AC_SLOT(6)  AC_SLOT(7)
        AC_SLOT(8)  AC_SLOT(9)  AC_SLOT(10) AC_SLOT(11)
        AC_SLOT(12) AC_SLOT(13) AC_SLOT(14) AC_SLOT(15)

        static void* g_slotFn[16] = {
            (void*)&hk_acm0,  (void*)&hk_acm1,  (void*)&hk_acm2,  (void*)&hk_acm3,
            (void*)&hk_acm4,  (void*)&hk_acm5,  (void*)&hk_acm6,  (void*)&hk_acm7,
            (void*)&hk_acm8,  (void*)&hk_acm9,  (void*)&hk_acm10, (void*)&hk_acm11,
            (void*)&hk_acm12, (void*)&hk_acm13, (void*)&hk_acm14, (void*)&hk_acm15,
    };
    static void** g_slotTramp[16] = {
        &o_acm0,  &o_acm1,  &o_acm2,  &o_acm3,
        &o_acm4,  &o_acm5,  &o_acm6,  &o_acm7,
        &o_acm8,  &o_acm9,  &o_acm10, &o_acm11,
        &o_acm12, &o_acm13, &o_acm14, &o_acm15,
    };

    static int HookFlagGetters(Il2CppClass* k) {
        int slot = 0, n = 0;
        for (Il2CppClass* c = k; c && slot < 16; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                const MethodInfo* m = il2cpp_class_get_methods(c, &it);
                if (!m || slot >= 16) break;
                if (il2cpp_method_get_param_count(m) != 0) continue;

                const char* rt = SDK::MethodReturnName((MethodInfo*)m);
                if (!rt || !strstr(rt, "Boolean")) continue;

                const char* mn = il2cpp_method_get_name(m);
                if (!mn) continue;
                char low[96]; Lower(mn, low, sizeof(low));
                if (!NameHas(low, { "speed", "fly", "noclip", "no_clip", "cheat", "hack",
                                    "block", "detect", "teleport", "suspect", "violation",
                                    "kick", "ban", "desync", "check" }))
                    continue;

                void* p = SDK::MethodPtr((MethodInfo*)m);
                if (!p) continue;

                bool dup = false;
                for (int j = 0; j < slot; ++j)
                    if (g_slotTramp[j] && *g_slotTramp[j] == p) dup = true;
                if (dup) continue;

                if (MH_CreateHook(p, g_slotFn[slot], g_slotTramp[slot]) == MH_OK) {
                    MH_EnableHook(p);
                    // >>> FIX: Il2CppClass e incompleto -> usa o getter
                    Log("[ac] getter neutralizado: %s::%s (%p)", ClassName(c), mn, p);
                    ++slot; ++n;
                }
            }
        }
        Log("[ac] getters forcados p/ false: %d", n);
        return n;
    }

    // ---------------------------------------------------------------- instancia
    static Il2CppObject* InstanceOnPlayer() {
        if (!SDK::g_localPlayer || !g_cls) return nullptr;
        Il2CppClass* pk = il2cpp_object_get_class(SDK::g_localPlayer);
        for (Il2CppClass* c = pk; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                const Il2CppType* t = il2cpp_field_get_type(f);
                if (!t) continue;
                Il2CppClass* ft = il2cpp_class_from_type ? il2cpp_class_from_type(t) : nullptr;
                if (!ft) continue;
                bool match = (ft == g_cls);
                for (Il2CppClass* p = ft; !match && p; p = il2cpp_class_get_parent(p))
                    if (p == g_cls) match = true;
                if (!match) continue;

                Il2CppObject* o = SafeRead<Il2CppObject*>(
                    (uintptr_t)SDK::g_localPlayer + il2cpp_field_get_offset(f), nullptr);
                if (o) {
                    Log("[ac] instancia no player: campo '%s' (%p)", il2cpp_field_get_name(f), o);
                    return o;
                }
            }
        }
        return nullptr;
    }

    static Il2CppObject* InstanceViaGetter() {
        MethodInfo* g = nullptr;
        for (Il2CppClass* c = g_cls; c && !g; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                const MethodInfo* m = il2cpp_class_get_methods(c, &it);
                if (!m) break;
                if (il2cpp_method_get_param_count(m) != 0) continue;
                const char* mn = il2cpp_method_get_name(m);
                if (!mn) continue;
                if (strcmp(mn, "get_Instance") == 0 || strcmp(mn, "get_instance") == 0 ||
                    strcmp(mn, "get_Current") == 0 || strcmp(mn, "get_Singleton") == 0) {
                    g = (MethodInfo*)m; break;
                }
            }
        }
        if (!g || !il2cpp_runtime_invoke) return nullptr;
        Il2CppException* ex = nullptr;
        Il2CppObject* o = il2cpp_runtime_invoke(g, nullptr, nullptr, &ex);
        if (o) Log("[ac] instancia via getter %s (%p)", il2cpp_method_get_name(g), o);
        return o;
    }

    static void FindInstance() {
        uint64_t now = GetTickCount64();
        if (now - g_lastTry < 1000) return;
        g_lastTry = now;

        static const char* cands[] = { "instance", "Instance", "m_Instance", "_instance",
                                       "singleton", "m_Singleton", "current", "m_Current",
                                       "s_Instance", "g_Instance" };
        for (const char* n : cands) {
            FieldInfo* f = il2cpp_class_get_field_from_name(g_cls, n);
            if (!f) continue;
            Il2CppObject* o = nullptr;
            __try { il2cpp_field_static_get_value(f, &o); }
            __except (EXCEPTION_EXECUTE_HANDLER) { o = nullptr; }
            if (o) { g_obj = o; Log("[ac] instancia via estatico '%s' (%p)", n, o); return; }
        }

        g_obj = InstanceViaGetter();
        if (g_obj) return;

        g_obj = InstanceOnPlayer();
    }

    // ---------------------------------------------------------------- limpeza
    static void ClearAll() {
        if (g_obj) {
            uintptr_t base = (uintptr_t)g_obj;
            for (auto& b : g_bools) WriteAt<bool>(base + b.off, false);
            for (auto& i : g_ints)  WriteAt<int>(base + i.off, 0);

            if (SDK::g_localPlayer) {
                Vec3 p = SDK::GetPos(SDK::g_localPlayer);
                for (auto& v : g_vecs) WriteAt<Vec3>(base + v.off, p);
            }
        }

        for (auto& s : g_statics) {
            __try {
                if (s.isBool) { bool b = false;  StaticSet(s.f, &b); }
                else { int32_t v = 0;   StaticSet(s.f, &v); }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        if (SDK::g_localPlayer && !g_bools.empty()) {
            uintptr_t pb = (uintptr_t)SDK::g_localPlayer;
            for (auto& b : g_bools) {
                __try { WriteAt<bool>(pb + b.off, false); }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        if (++g_cleared % 300 == 0)
            Log("[ac] limpo (obj=%p bools=%d ints=%d vecs=%d statics=%d)",
                g_obj, (int)g_bools.size(), (int)g_ints.size(),
                (int)g_vecs.size(), (int)g_statics.size());
    }

    typedef void(__fastcall* tAcUpd)(void* self, void* method);
    static void __fastcall hk_acUpdate(void* self, void* method) {
        ClearAll();
        if (o_acUpdate) ((tAcUpd)o_acUpdate)(self, method);
        ClearAll();
    }

    static void HookAcUpdates() {
        static const char* ms[] = { "Update", "FixedUpdate", "LateUpdate" };
        for (const char* name : ms) {
            MethodInfo* m = nullptr;
            for (Il2CppClass* c = g_cls; c && !m; c = il2cpp_class_get_parent(c)) {
                void* it = nullptr;
                for (;;) {
                    const MethodInfo* f = il2cpp_class_get_methods(c, &it);
                    if (!f) break;
                    if (il2cpp_method_get_param_count(f) == 0 &&
                        strcmp(il2cpp_method_get_name(f), name) == 0) {
                        m = (MethodInfo*)f; break;
                    }
                }
            }
            if (!m) continue;
            void* p = SDK::MethodPtr(m);
            if (!p) continue;
            if (MH_CreateHook(p, &hk_acUpdate, (void**)&o_acUpdate) == MH_OK) {
                MH_EnableHook(p);
                Log("[ac] hook %s OK (%p) — limpa antes/depois", name, p);
                return;
            }
        }
        Log("[ac] nenhum Update do anti-cheat encontrado (fica so o Tick)");
    }

    // ---------------------------------------------------------------- install
    bool Install() {
        ResolveAcApi();          // >>> FIX: antes de qualquer uso

        static const char* names[] = { "AntiHack", "AntiCheat", "AntiCheatSystem",
                                       "CheatDetector", "HackDetector", "ACSystem",
                                       "PlayerAntiCheat", "Anticheat", "AntiHackSystem" };
        for (const char* n : names) {
            g_cls = SDK::FindClass(n);
            if (g_cls) { Log("[ac] classe = %s", n); break; }
        }
        if (!g_cls) g_cls = SDK::kAntiHack;
        if (!g_cls) { Log("[ac] classe do anti-cheat NAO encontrada"); return false; }

        ScanFields(g_cls, "campos");
        ScanStatics(g_cls);
        HookFlagGetters(g_cls);
        HookAcUpdates();
        FindInstance();

        Log("[ac] Install ok cls=%p obj=%p", g_cls, g_obj);
        return true;
    }

    void Tick() {
        PHASE("AntiCheat::Tick");
        if (!C.antiCheat) return;
        if (!g_cls) return;
        if (!g_obj) FindInstance();

        if (g_obj || !g_statics.empty()) ClearAll();

        ++g_ticks;
    }

} // namespace AntiCheat