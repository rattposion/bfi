// ============================================================================
//  unlock.cpp — bfi (v12)
//
//  >>> FIX v12 (o FPS de 1 ao entrar na partida):
//   1) BuildCamoList() varria a IMAGEM INTEIRA de classes e era chamado por
//      CamoCount() e CamoList() — que o menu chama a cada frame. E como os
//      camos nao eram achados, g_camoReal ficava false e ele varria DE NOVO
//      toda vez. Agora tem cache (g_camoBuilt) e o rescan so acontece no Tick,
//      no maximo 4 vezes por partida.
//   2) ApplyPlayerInts() resolvia os offsets de moeda/nivel com
//      il2cpp_type_get_name em ~200 campos do PlayerScript A CADA FRAME.
//      Agora resolve uma vez e depois so escreve.
//   3) ApplyAll() roda a cada 15 ticks (era todo frame): 130 writes + a
//      iteracao de campos viram 1/15 do custo.
//   4) Log de progresso a cada 600 ticks, pra nao spammar arquivo.
//
//  Mantido: loadout via fpsGunView (0x48 arma / 0x4C mira / 0x50 cano /
//  0x54 acessorio), anexos List<enum> stride 4, FindArray validando o
//  elemento 0 == gunStats, __try no Tick, char field logado mesmo null.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"
#include "memory.h"
#include "il2cpp_api.h"
#include "unlock.h"

#include <windows.h>
#include <cstring>
#include <string>
#include <vector>

#define BFI_UNLOCK_VER "v12"

namespace Unlock {

    // ================================================================== estado
    static void* g_arr = nullptr;
    static int    g_arrLen = 0;
    static uint32_t g_offArr = 0;

    static uint32_t g_offName = 0, g_offAvailable = 0, g_offCamoType = 0;
    static uint32_t g_offTracerType = 0, g_offGcs = 0;
    static uint32_t g_offSights = 0, g_offAcc = 0, g_offBarrels = 0;
    static uint32_t g_offFpsGunView = 0;

    static const uint32_t kFvWeapon = 0x48;
    static const uint32_t kFvSights = 0x4C;
    static const uint32_t kFvBarrels = 0x50;
    static const uint32_t kFvAcc = 0x54;

    static bool   g_installed = false;
    static bool   g_listsDone = false;
    static uint32_t g_reapply = 0;
    static uint32_t g_retry = 0;

    // cache dos scans caros
    static bool   g_camoBuilt = false;
    static bool   g_camoReal = false;
    static int    g_camoTries = 0;

    static const char* kSlotNames[3] = { "Mira (sights)", "Acessorios", "Cano (barrels)" };

    static std::vector<std::string> g_wpnStr, g_camoStr, g_charStr;
    static std::vector<std::string> g_atStr[3];
    static std::vector<const char*> g_wpnPtr, g_camoPtr, g_charPtr, g_atPtr[3];

    // ================================================================= helpers
    typedef Il2CppClass* (*fn_elem)(Il2CppClass*);
    typedef uint32_t(*fn_arrlen)(const Il2CppArray*);
    typedef uint32_t(*fn_field_flags)(FieldInfo*);
    typedef void     (*fn_field_static_get)(FieldInfo*, void*);
    typedef void* (*fn_class_get_image)(Il2CppClass*);
    typedef size_t(*fn_image_class_count)(const void*);
    typedef Il2CppClass* (*fn_image_get_class)(const void*, size_t);

    static fn_elem              p_elem = nullptr;
    static fn_arrlen            p_arrlen = nullptr;
    static fn_field_flags       p_fieldFlags = nullptr;
    static fn_field_static_get  p_fieldStaticGet = nullptr;
    static fn_class_get_image   p_classImage = nullptr;
    static fn_image_class_count p_imgCount = nullptr;
    static fn_image_get_class   p_imgClass = nullptr;

    static void ResolveExtra() {
        static bool done = false;
        if (done) return;
        done = true;
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");
        if (!ga) return;
        p_elem = (fn_elem)GetProcAddress(ga, "il2cpp_class_get_element_class");
        p_arrlen = (fn_arrlen)GetProcAddress(ga, "il2cpp_array_length");
        p_fieldFlags = (fn_field_flags)GetProcAddress(ga, "il2cpp_field_get_flags");
        p_fieldStaticGet = (fn_field_static_get)GetProcAddress(ga, "il2cpp_field_static_get_value");
        p_classImage = (fn_class_get_image)GetProcAddress(ga, "il2cpp_class_get_image");
        p_imgCount = (fn_image_class_count)GetProcAddress(ga, "il2cpp_image_get_class_count");
        p_imgClass = (fn_image_get_class)GetProcAddress(ga, "il2cpp_image_get_class");
        Log("[unlock] api extra: elem=%d arrlen=%d flags=%d static_get=%d image=%d",
            (int)(p_elem != nullptr), (int)(p_arrlen != nullptr),
            (int)(p_fieldFlags != nullptr), (int)(p_fieldStaticGet != nullptr),
            (int)(p_classImage != nullptr));
    }

    static bool Plausible(const void* p) {
        uintptr_t a = (uintptr_t)p;
        if (a < 0x10000 || a > 0x00007FFFFFFFFFFFULL) return false;
        if (a & 7) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        return true;
    }
    static bool LooksLikeObj(const void* p) {
        if (!Plausible(p)) return false;
        return Plausible(*(Il2CppClass**)p);
    }
    static bool IsStatic(FieldInfo* f) {
        return (p_fieldFlags && f) ? ((p_fieldFlags(f) & 0x10u) != 0u) : false;
    }

    static int ArrLen(const Il2CppArray* a) {
        if (!a) return 0;
        if (p_arrlen) return (int)p_arrlen(a);
        return *(int*)((uint8_t*)a + 0x18);
    }
    static void* ArrElem(const Il2CppArray* a, int i) {
        if (!a || i < 0) return nullptr;
        return *(void**)((uint8_t*)a + 0x20 + (size_t)i * sizeof(void*));
    }

    static void* ListItems(void* list) { return list ? SafeRead<void*>((uintptr_t)list + 0x10, nullptr) : nullptr; }
    static int   ListSize(void* list) { return list ? SafeRead<int>((uintptr_t)list + 0x18, 0) : 0; }
    static int   ListElemInt(void* list, int i) {
        void* items = ListItems(list);
        if (!items || i < 0) return -1;
        return SafeRead<int>((uintptr_t)items + 0x20 + (size_t)i * 4, -1);
    }

    struct BStr { void* klass; void* monitor; int32_t len; uint16_t ch[1]; };
    static void CStr(const void* s, char* out, size_t n) {
        out[0] = 0;
        if (!LooksLikeObj(s)) return;
        const BStr* b = (const BStr*)s;
        if (b->len <= 0 || b->len > 256) return;
        size_t i = 0;
        for (; i < (size_t)b->len && i + 1 < n; ++i)
            out[i] = (b->ch[i] < 128) ? (char)b->ch[i] : '?';
        out[i] = 0;
    }

    static void Lower(const char* s, char* out, size_t n) {
        size_t i = 0;
        for (; s && s[i] && i + 1 < n; ++i) {
            char c = s[i];
            out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        out[i] = 0;
    }
    static bool NameHasAny(const char* low, const char* const* keys, int n) {
        for (int i = 0; i < n; ++i) if (strstr(low, keys[i])) return true;
        return false;
    }
    static void AddUniq(std::vector<std::string>& v, const char* s) {
        if (!s || !s[0]) return;
        for (size_t i = 0; i < v.size(); ++i) if (v[i] == s) return;
        v.push_back(s);
    }
    static void BuildPtrs(std::vector<std::string>& src, std::vector<const char*>& dst) {
        dst.clear();
        dst.reserve(src.size());
        for (size_t i = 0; i < src.size(); ++i) dst.push_back(src[i].c_str());
    }

    static uint32_t FieldOffNamed(Il2CppClass* k, const char* name) {
        if (!k) return 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            FieldInfo* f = il2cpp_class_get_field_from_name(c, name);
            if (f) return (uint32_t)il2cpp_field_get_offset(f);
        }
        return 0;
    }
    static uint32_t FieldOffByType(Il2CppClass* k, const char* key) {
        if (!k) return 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (!tn || !strstr(tn, key)) continue;
                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (off) return off;
            }
        }
        return 0;
    }

    // =============================================================== descoberta
    static bool FindWeaponArray(void* p, uint32_t& outOff, void*& outArr, int& outLen) {
        outOff = 0; outArr = nullptr; outLen = 0;
        if (!p || !SDK::kPlayerScript || !SDK::kGunStats) return false;

        uint32_t bestOff = 0; void* bestArr = nullptr; int bestLen = 0;
        for (Il2CppClass* c = SDK::kPlayerScript; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                if (!t) continue;
                const char* tn = il2cpp_type_get_name ? il2cpp_type_get_name(t) : nullptr;
                if (!tn || !strstr(tn, "[]")) continue;

                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (!off) continue;
                void* a = SafeRead<void*>((uintptr_t)p + off, nullptr);
                if (!LooksLikeObj(a)) continue;

                int len = ArrLen((Il2CppArray*)a);
                if (len < 8 || len > 512) continue;

                void* e0 = ArrElem((Il2CppArray*)a, 0);
                if (!e0 || !LooksLikeObj(e0)) continue;
                Il2CppClass* k = il2cpp_object_get_class((Il2CppObject*)e0);
                bool ok = (k == SDK::kGunStats);
                for (Il2CppClass* q = k; !ok && q; q = il2cpp_class_get_parent(q))
                    if (q == SDK::kGunStats) ok = true;
                if (!ok) continue;

                if (len > bestLen) { bestOff = off; bestArr = a; bestLen = len; }
            }
        }
        if (!bestArr) return false;
        outOff = bestOff; outArr = bestArr; outLen = bestLen;
        return true;
    }

    static void ResolveOffsets() {
        if (!SDK::kGunStats) return;
        g_offName = FieldOffNamed(SDK::kGunStats, "weaponName");
        g_offAvailable = FieldOffNamed(SDK::kGunStats, "available");
        g_offCamoType = FieldOffNamed(SDK::kGunStats, "camoType");
        g_offTracerType = FieldOffNamed(SDK::kGunStats, "tracerType");
        g_offGcs = FieldOffNamed(SDK::kGunStats, "gunCustomizeScript");
        g_offSights = FieldOffNamed(SDK::kGunStats, "possibleSights");
        g_offAcc = FieldOffNamed(SDK::kGunStats, "possibleAccessories");
        g_offBarrels = FieldOffNamed(SDK::kGunStats, "possibleBarrels");

        if (!g_offName)       g_offName = 0x278;
        if (!g_offAvailable)  g_offAvailable = 0x270;
        if (!g_offCamoType)   g_offCamoType = 0x2C0;
        if (!g_offTracerType) g_offTracerType = 0x2C4;
        if (!g_offGcs)        g_offGcs = 0x2B0;
        if (!g_offSights)     g_offSights = 0x118;
        if (!g_offAcc)        g_offAcc = 0x120;
        if (!g_offBarrels)    g_offBarrels = 0x128;

        if (SDK::kPlayerScript) {
            g_offFpsGunView = FieldOffNamed(SDK::kPlayerScript, "fpsGunView");
            if (!g_offFpsGunView) g_offFpsGunView = FieldOffByType(SDK::kPlayerScript, "FirstPersonGunView");
            if (!g_offFpsGunView) g_offFpsGunView = 0x8F0;
        }

        Log("[unlock] offsets: nome=0x%X avail=0x%X camo=0x%X gcs=0x%X "
            "sights=0x%X acc=0x%X barrels=0x%X fpsGunView=0x%X",
            g_offName, g_offAvailable, g_offCamoType, g_offGcs,
            g_offSights, g_offAcc, g_offBarrels, g_offFpsGunView);
    }

    static void* FpsGunView() {
        if (!SDK::g_localPlayer || !g_offFpsGunView) return nullptr;
        void* v = SafeRead<void*>((uintptr_t)SDK::g_localPlayer + g_offFpsGunView, nullptr);
        return LooksLikeObj(v) ? v : nullptr;
    }

    // ==================================================================== camos
    static const char* kCamoFallback[] = {
        "Nenhuma","Artico","Deserto","Floresta","Urbano","Digital","Neve",
        "Vermelho","Azul","Verde","Amarelo","Rosa","Ciano","Magenta",
        "Ouro","Prata","Bronze","Platina","Diamante","Rubi","Esmeralda","Safira",
        "Fogo","Gelo","Toxic","Blood","Fantasma","Neon","Cyber","Vapor",
        "Camuflado","Carbono","Fibra","Titanio","Cromado","Ouro Negro",
        "Caveira","Caveira Ouro","Serpente","Dragao","Lobo","Falcao",
        "Estelar","Galaxia","Nebulosa","Aurora","Meteoro","Vulcao","Glacial",
        "Halloween","Natal","Lendario","Epico","Raro","Comum"
    };

    static void CollectStringsFromClasses(const char* key,
        std::vector<std::string>& out, const char* tag) {
        if (!p_classImage || !p_imgCount || !p_imgClass || !SDK::kPlayerScript) return;
        void* img = p_classImage(SDK::kPlayerScript);
        if (!img) { Log("[unlock] %s: sem imagem", tag); return; }

        size_t n = p_imgCount(img);
        if (n > 60000) n = 60000;
        int classes = 0, campos = 0;

        for (size_t i = 0; i < n; ++i) {
            Il2CppClass* c = p_imgClass(img, i);
            if (!c) continue;
            const char* cn = il2cpp_class_get_name(c);
            if (!cn) continue;
            char low[128]; Lower(cn, low, sizeof(low));
            if (!strstr(low, key)) continue;
            ++classes;

            for (Il2CppClass* k = c; k; k = il2cpp_class_get_parent(k)) {
                void* it = nullptr;
                for (;;) {
                    FieldInfo* f = il2cpp_class_get_fields(k, &it);
                    if (!f) break;
                    const Il2CppType* t = il2cpp_field_get_type(f);
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                    if (!tn || !strstr(tn, "String")) continue;
                    if (!strstr(tn, "[]") && !strstr(tn, "List<")) continue;
                    ++campos;

                    void* v = nullptr;
                    if (IsStatic(f) && p_fieldStaticGet) {
                        __try { p_fieldStaticGet(f, &v); }
                        __except (EXCEPTION_EXECUTE_HANDLER) { v = nullptr; }
                    }
                    if (!LooksLikeObj(v)) {
                        Log("[unlock] %s: %s::%s %s = null%s", tag, cn,
                            il2cpp_field_get_name(f), tn,
                            IsStatic(f) ? "" : " (campo de INSTANCIA)");
                        continue;
                    }

                    bool isArr = (strstr(tn, "[]") != nullptr);
                    int cnt = isArr ? ArrLen((Il2CppArray*)v) : ListSize(v);
                    Log("[unlock] %s: %s::%s -> %d itens", tag, cn,
                        il2cpp_field_get_name(f), cnt);

                    for (int j = 0; j < cnt && j < 256; ++j) {
                        void* e = isArr ? ArrElem((Il2CppArray*)v, j)
                            : ArrElem((Il2CppArray*)ListItems(v), j);
                        char s[128]; CStr(e, s, sizeof(s));
                        AddUniq(out, s);
                    }
                }
            }
        }
        Log("[unlock] %s: %d classes com '%s' (de %d), %d campos String[]/List<string>",
            tag, classes, key, (int)n, campos);
    }

    // >>> FIX v12: cache. 'force' so e usado pelo Dump e pelo retry do Tick.
    static void BuildCamoList(bool force) {
        if (g_camoBuilt && !force) return;
        if (g_camoReal && !force) return;

        std::vector<std::string> tmp;
        CollectStringsFromClasses("camo", tmp, "camos");
        if (tmp.empty()) CollectStringsFromClasses("camosdata", tmp, "camos");
        if (tmp.empty()) CollectStringsFromClasses("skin", tmp, "camos");

        if (!tmp.empty()) {
            g_camoStr = tmp;
            g_camoReal = true;
        }
        else if (g_camoStr.empty()) {
            for (size_t i = 0; i < sizeof(kCamoFallback) / sizeof(kCamoFallback[0]); ++i)
                g_camoStr.push_back(kCamoFallback[i]);
            Log("[unlock] camos: FALLBACK");
        }
        BuildPtrs(g_camoStr, g_camoPtr);
        g_camoBuilt = true;
        Log("[unlock] lista de camos: %d (real=%d)", (int)g_camoStr.size(), (int)g_camoReal);
    }

    // ==================================================================== chars
    static void* g_charArr = nullptr;
    static int      g_charLen = 0;
    static bool     g_charIsList = false;
    static bool     g_charLogged = false;

    static void* CharElem(int i) {
        if (!g_charArr || i < 0 || i >= g_charLen) return nullptr;
        if (g_charIsList) return ArrElem((Il2CppArray*)ListItems(g_charArr), i);
        return ArrElem((Il2CppArray*)g_charArr, i);
    }

    static bool FindCharArray() {
        if (!SDK::kPlayerScript || !SDK::g_localPlayer) return false;
        if (g_charArr && LooksLikeObj(g_charArr) && g_charLen > 0) return true;

        for (Il2CppClass* c = SDK::kPlayerScript; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (!tn || !strstr(tn, "CharacterCustom")) continue;

                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (!off) continue;

                void* v = SafeRead<void*>((uintptr_t)SDK::g_localPlayer + off, nullptr);
                bool isList = (strstr(tn, "List<") != nullptr);
                int len = LooksLikeObj(v) ? (isList ? ListSize(v) : ArrLen((Il2CppArray*)v)) : 0;

                if (!g_charLogged) {
                    g_charLogged = true;
                    Log("[unlock] char field '%s' %s @0x%X = %p len=%d",
                        il2cpp_field_get_name(f), tn, off, v, len);
                }
                if (!LooksLikeObj(v) || len <= 0 || len > 64) continue;

                g_charArr = v;
                g_charLen = len;
                g_charIsList = isList;
                Log("[unlock] chars: %d instancias via '%s' (%s)",
                    len, il2cpp_field_get_name(f), isList ? "List" : "array");
                return true;
            }
        }
        return false;
    }

    static bool CharNameOf(void* inst, char* out, size_t n) {
        out[0] = 0;
        if (!LooksLikeObj(inst)) return false;
        Il2CppClass* k = il2cpp_object_get_class((Il2CppObject*)inst);
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (!tn || !strstr(tn, "String")) continue;
                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (!off) continue;
                char tmp[128];
                CStr(SafeRead<void*>((uintptr_t)inst + off, nullptr), tmp, sizeof(tmp));
                if (tmp[0]) { strncpy_s(out, n, tmp, _TRUNCATE); return true; }
            }
        }
        return false;
    }

    static void BuildCharList() {
        if (!FindCharArray()) return;

        g_charStr.clear();
        for (int i = 0; i < g_charLen; ++i) {
            void* inst = CharElem(i);
            char nm[128]; nm[0] = 0;
            if (!CharNameOf(inst, nm, sizeof(nm))) sprintf_s(nm, "Personagem %d", i);
            char line[160]; sprintf_s(line, "%02d - %s", i, nm);
            g_charStr.push_back(line);
        }
        BuildPtrs(g_charStr, g_charPtr);
        Log("[unlock] lista de personagens: %d", (int)g_charStr.size());
    }

    // =================================================================== listas
    static void ResolveLists() {
        if (g_listsDone) return;
        if (!SDK::g_localPlayer || !g_arr || g_arrLen <= 0) return;
        g_listsDone = true;

        g_wpnStr.clear();
        for (int i = 0; i < g_arrLen; ++i) {
            void* gun = ArrElem((Il2CppArray*)g_arr, i);
            char nm[128]; nm[0] = 0;
            if (gun) CStr(SafeRead<void*>((uintptr_t)gun + g_offName, nullptr), nm, sizeof(nm));
            char line[160];
            if (nm[0]) sprintf_s(line, "%02d - %s", i, nm);
            else       sprintf_s(line, "%02d - Arma %d", i, i);
            g_wpnStr.push_back(line);
        }
        BuildPtrs(g_wpnStr, g_wpnPtr);
        Log("[unlock] lista de armas: %d", (int)g_wpnStr.size());

        void* gun0 = ArrElem((Il2CppArray*)g_arr, 0);
        const uint32_t offs[3] = { g_offSights, g_offAcc, g_offBarrels };
        for (int s = 0; s < 3; ++s) {
            g_atStr[s].clear();
            void* list = (gun0 && offs[s]) ? SafeRead<void*>((uintptr_t)gun0 + offs[s], nullptr) : nullptr;
            int cnt = LooksLikeObj(list) ? ListSize(list) : 0;
            for (int i = 0; i < cnt && i < 64; ++i) {
                char line[64];
                sprintf_s(line, "%s %d (valor %d)", kSlotNames[s], i, ListElemInt(list, i));
                g_atStr[s].push_back(line);
            }
            BuildPtrs(g_atStr[s], g_atPtr[s]);
            Log("[unlock] anexos slot %s: %d", kSlotNames[s], (int)g_atStr[s].size());
        }

        BuildCamoList(false);     // 1x
        BuildCharList();

        if (g_charStr.empty()) {
            for (int i = 0; i < 8; ++i) {
                char line[64]; sprintf_s(line, "Personagem %d", i);
                g_charStr.push_back(line);
            }
            BuildPtrs(g_charStr, g_charPtr);
            Log("[unlock] chars: FALLBACK (o array ainda nao existe)");
        }

        Log("[unlock] listas: armas=%d camos=%d chars=%d",
            (int)g_wpnPtr.size(), (int)g_camoPtr.size(), (int)g_charPtr.size());
    }

    // ==================================================================== apply
    static void ApplyAvailable() {
        if (!C.unlockWeapons || !g_arr || !g_offAvailable) return;
        for (int i = 0; i < g_arrLen; ++i) {
            void* gun = ArrElem((Il2CppArray*)g_arr, i);
            if (gun) SafeWrite<uint8_t>((uintptr_t)gun + g_offAvailable, 1);
        }
    }

    static void ApplyAttachments() {
        if (!g_arr || !C.unlockSkins || !g_offCamoType) return;
        int camo = (C.selCamo >= 0) ? C.selCamo : C.skinIndex;
        for (int i = 0; i < g_arrLen; ++i) {
            void* gun = ArrElem((Il2CppArray*)g_arr, i);
            if (!gun) continue;
            SafeWrite<int>((uintptr_t)gun + g_offCamoType, camo);
            if (g_offTracerType) SafeWrite<int>((uintptr_t)gun + g_offTracerType, camo);
        }
    }

    // >>> FIX v12: offsets resolvidos UMA vez (antes: il2cpp_type_get_name em
    //     ~200 campos do PlayerScript a cada frame).
    static uint32_t g_moneyOff[8]; static int g_moneyN = 0;
    static uint32_t g_rankOff[8];  static int g_rankN = 0;
    static bool     g_playerIntsDone = false;

    static void ResolvePlayerInts() {
        if (g_playerIntsDone || !SDK::kPlayerScript) return;
        g_playerIntsDone = true;
        static const char* moneyKeys[] = { "money", "cash", "coin", "credit", "gold", "currency" };
        static const char* rankKeys[] = { "level", "rank", "exp", "xp" };

        for (Il2CppClass* c = SDK::kPlayerScript; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (!tn || !strstr(tn, "Int32")) continue;
                const char* fn = il2cpp_field_get_name(f);
                if (!fn) continue;
                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                if (!off) continue;

                char low[96]; Lower(fn, low, sizeof(low));
                if (g_moneyN < 8 && NameHasAny(low, moneyKeys, 6)) g_moneyOff[g_moneyN++] = off;
                if (g_rankN < 8 && NameHasAny(low, rankKeys, 4))  g_rankOff[g_rankN++] = off;
            }
        }
        Log("[unlock] ints do player: moeda=%d nivel=%d", g_moneyN, g_rankN);
    }

    static void ApplyPlayerInts() {
        if (!SDK::g_localPlayer) return;
        if (!g_playerIntsDone) ResolvePlayerInts();
        uintptr_t p = (uintptr_t)SDK::g_localPlayer;
        if (C.unlockCurrency)
            for (int i = 0; i < g_moneyN; ++i) SafeWrite<int>(p + g_moneyOff[i], C.currencyValue);
        if (C.unlockLevel)
            for (int i = 0; i < g_rankN; ++i) SafeWrite<int>(p + g_rankOff[i], C.unlockLevel);
    }

    static void ApplySelection() {
        if (!C.applySelection || !SDK::g_localPlayer) return;
        void* fv = FpsGunView();
        if (!fv) return;

        if (C.selWeapon >= 0) {
            SafeWrite<int>((uintptr_t)fv + kFvWeapon, C.selWeapon);
            if (C.unlockSkins && g_offCamoType) {
                void* gun = ArrElem((Il2CppArray*)g_arr, C.selWeapon);
                if (gun) SafeWrite<int>((uintptr_t)gun + g_offCamoType,
                    (C.selCamo >= 0) ? C.selCamo : C.skinIndex);
            }
        }

        const uint32_t slotOff[3] = { kFvSights, kFvAcc, kFvBarrels };
        for (int s = 0; s < 3; ++s) {
            int idx = C.selAttach[s];
            if (idx < 0) continue;
            int val = AttachValue(s, idx);
            if (val < 0) continue;
            SafeWrite<int>((uintptr_t)fv + slotOff[s], val);
        }
    }

    static void ApplyAll() {
        if (!g_arr || g_arrLen <= 0) return;
        if (C.unlockAll || C.unlockWeapons) ApplyAvailable();
        if (C.unlockAll || C.unlockSkins || C.skinChanger) ApplyAttachments();
        if (C.unlockAll || C.unlockCurrency || C.unlockLevel) ApplyPlayerInts();
        if (C.applySelection) ApplySelection();
    }

    // ==================================================================== public
    bool Install() {
        ResolveExtra();
        if (!SDK::g_localPlayer || !SDK::kPlayerScript || !SDK::kGunStats) return false;

        uint32_t off = 0; void* a = nullptr; int len = 0;
        if (!FindWeaponArray(SDK::g_localPlayer, off, a, len)) return false;

        g_offArr = off; g_arr = a; g_arrLen = len;
        ResolveOffsets();
        g_listsDone = false;
        ResolveLists();

        g_installed = true;
        Log("[unlock] install %s: arr@0x%X len=%d avail=0x%X camo=0x%X gcs=0x%X "
            "nome=0x%X fpsGunView=0x%X armas=%d camos=%d chars=%d at[%d/%d/%d]",
            BFI_UNLOCK_VER, g_offArr, g_arrLen, g_offAvailable, g_offCamoType, g_offGcs,
            g_offName, g_offFpsGunView, (int)g_wpnStr.size(), (int)g_camoStr.size(),
            (int)g_charStr.size(), (int)g_atStr[0].size(), (int)g_atStr[1].size(),
            (int)g_atStr[2].size());
        return true;
    }

    void Tick() {
        if (!C.unlockAll && !C.unlockWeapons && !C.unlockSkins && !C.unlockAttachments &&
            !C.unlockCurrency && !C.unlockLevel && !C.unlockChars && !C.applySelection &&
            !C.skinChanger) return;
        if (!SDK::g_localPlayer) return;

        __try {
            if (!g_installed || !g_arr || g_arrLen <= 0) {
                if (!Install()) return;
            }
            if (!LooksLikeObj(g_arr)) { g_installed = false; return; }

            ++g_reapply;

            // >>> FIX v12: 1x a cada 15 ticks em vez de todo frame
            if ((g_reapply % 15) == 0) ApplyAll();

            // retry das listas caras: no MAXIMO 4 vezes por partida
            if ((++g_retry % 600) == 0) {
                if (!g_camoReal && g_camoTries < 4) { ++g_camoTries; BuildCamoList(true); }
                if (g_charStr.empty()) BuildCharList();
            }

            if ((g_reapply % 600) == 0)
                Log("[unlock] vivo %u (arr@0x%X len=%d camos=%d chars=%d)",
                    g_reapply, g_offArr, g_arrLen, (int)g_camoStr.size(), (int)g_charStr.size());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            static int n = 0;
            if (n++ < 10) Log("[unlock] AV contido no Tick");
        }
    }

    // --------------------------------------------------------------------- dump
    static void DumpClassFields(const char* clsName) {
        Il2CppClass* k = SDK::FindClass(clsName);
        if (!k) { Log("[dump] classe '%s' NAO encontrada", clsName); return; }
        Log("[dump] ==== %s ====", clsName);
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : "?";
                Log("[dump]   %-34s %-46s @0x%X%s",
                    il2cpp_field_get_name(f), tn ? tn : "?",
                    (unsigned)il2cpp_field_get_offset(f),
                    IsStatic(f) ? " [static]" : "");
            }
        }
    }

    static void DumpFields(void* obj, const char* tag) {
        if (!LooksLikeObj(obj)) return;
        Log("[dump:%s] ---- %p ----", tag, obj);
        Il2CppClass* k = il2cpp_object_get_class((Il2CppObject*)obj);
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = il2cpp_class_get_fields(c, &it);
                if (!f) break;
                if (IsStatic(f)) continue;
                const Il2CppType* t = il2cpp_field_get_type(f);
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : "?";
                uint32_t off = (uint32_t)il2cpp_field_get_offset(f);
                const char* fn = il2cpp_field_get_name(f);
                if (tn && strstr(tn, "Int32"))
                    Log("[dump:%s]   %-32s @0x%X = %d", tag, fn, off,
                        SafeRead<int>((uintptr_t)obj + off, -999));
                else if (tn && strstr(tn, "String")) {
                    char v[128]; CStr(SafeRead<void*>((uintptr_t)obj + off, nullptr), v, sizeof(v));
                    Log("[dump:%s]   %-32s @0x%X = \"%s\"", tag, fn, off, v);
                }
                else if (tn && strstr(tn, "Boolean"))
                    Log("[dump:%s]   %-32s @0x%X = %d", tag, fn, off,
                        (int)SafeRead<uint8_t>((uintptr_t)obj + off, 2));
                else
                    Log("[dump:%s]   %-32s %s @0x%X", tag, fn, tn, off);
            }
        }
    }

    void Dump() {
        ResolveExtra();
        Log("=== DUMP UNLOCK %s ===", BFI_UNLOCK_VER);
        Log("[dump] local=%p kGunStats=%p kPlayerScript=%p",
            SDK::g_localPlayer, SDK::kGunStats, SDK::kPlayerScript);

        if (!SDK::g_localPlayer)
            Log("[dump] SEM LOCAL PLAYER — aperte de novo DENTRO da partida");
        if (!g_installed) Install();

        Log("[dump] arr@0x%X len=%d", g_offArr, g_arrLen);
        for (int i = 0; i < g_arrLen && i < 8; ++i) {
            void* gun = ArrElem((Il2CppArray*)g_arr, i);
            char nm[128]; nm[0] = 0;
            if (gun) CStr(SafeRead<void*>((uintptr_t)gun + g_offName, nullptr), nm, sizeof(nm));
            Log("[dump] ARMASA [%02d] \"%s\" available=%d", i, nm[0] ? nm : "?",
                gun ? (int)SafeRead<uint8_t>((uintptr_t)gun + g_offAvailable, 0) : -1);
        }

        void* gun0 = ArrElem((Il2CppArray*)g_arr, 0);
        const uint32_t offs[3] = { g_offSights, g_offAcc, g_offBarrels };
        for (int s = 0; s < 3; ++s) {
            void* list = (gun0 && offs[s]) ? SafeRead<void*>((uintptr_t)gun0 + offs[s], nullptr) : nullptr;
            int cnt = LooksLikeObj(list) ? ListSize(list) : 0;
            Log("[dump] ANEXOS %s @0x%X: %d valores:", kSlotNames[s], offs[s], cnt);
            for (int i = 0; i < cnt && i < 32; ++i)
                Log("[dump]   [%d] valor=%d", i, ListElemInt(list, i));
        }

        BuildCamoList(true);            // rescan so aqui
        Log("[dump] camos montados: %d (real=%d)", (int)g_camoStr.size(), (int)g_camoReal);

        FindCharArray();
        Log("[dump] CHARACTERS len=%d", g_charLen);
        for (int i = 0; i < g_charLen && i < 8; ++i) {
            void* inst = CharElem(i);
            char nm[128]; nm[0] = 0;
            CharNameOf(inst, nm, sizeof(nm));
            Log("[dump] PERSONAGENS [%d] %p nome=\"%s\"", i, inst, nm);
            DumpFields(inst, "char");
        }

        Log("[dump] --- campos das classes candidatas ---");
        DumpClassFields("AllCamos");
        DumpClassFields("CamoManager");
        DumpClassFields("CamoDataScriptableObject");
        DumpClassFields("AB_CamosDataArray");
        DumpClassFields("UnlockedPlayerCamosScriptableObject");
        DumpClassFields("CharacterCustomizationsReferences");
        DumpClassFields("CharacterDresser");
        DumpClassFields("CharacterSkinTextureSettings");
        DumpClassFields("AttachmentManager");

        Log("=== FIM DUMP UNLOCK ===");
    }

    // ---------------------------------------------------------------------- API
    // >>> FIX v12: os accessors NAO chamam mais o scan caro.
    int WeaponCount() { ResolveLists(); return (int)g_wpnPtr.size(); }
    const char* const* WeaponList() { ResolveLists(); return g_wpnPtr.empty() ? nullptr : g_wpnPtr.data(); }

    int AttachSlotCount() { return 3; }
    const char* AttachSlotName(int slot) { return (slot >= 0 && slot < 3) ? kSlotNames[slot] : "?"; }

    int AttachCount(int slot) {
        if (slot < 0 || slot > 2 || !g_arr) return 0;
        const uint32_t offs[3] = { g_offSights, g_offAcc, g_offBarrels };
        void* gun = ArrElem((Il2CppArray*)g_arr, 0);
        if (!gun || !offs[slot]) return 0;
        void* list = SafeRead<void*>((uintptr_t)gun + offs[slot], nullptr);
        return LooksLikeObj(list) ? ListSize(list) : 0;
    }
    const char* const* AttachList(int slot) {
        ResolveLists();
        if (slot < 0 || slot > 2) return nullptr;
        return g_atPtr[slot].empty() ? nullptr : g_atPtr[slot].data();
    }
    int AttachValue(int slot, int idx) {
        if (slot < 0 || slot > 2 || !g_arr) return -1;
        const uint32_t offs[3] = { g_offSights, g_offAcc, g_offBarrels };
        void* gun = ArrElem((Il2CppArray*)g_arr, 0);
        if (!gun || !offs[slot]) return -1;
        void* list = SafeRead<void*>((uintptr_t)gun + offs[slot], nullptr);
        if (!LooksLikeObj(list)) return -1;
        return ListElemInt(list, idx);
    }

    int CamoCount() { if (!g_camoBuilt) BuildCamoList(false); return (int)g_camoPtr.size(); }
    const char* const* CamoList() { if (!g_camoBuilt) BuildCamoList(false); return g_camoPtr.empty() ? nullptr : g_camoPtr.data(); }

    int CharCount() { ResolveLists(); return (int)g_charPtr.size(); }
    const char* const* CharList() { ResolveLists(); return g_charPtr.empty() ? nullptr : g_charPtr.data(); }

} // namespace Unlock
// ==== FIM unlock.cpp ====