// unlock.cpp - bfi
#include "common.h"
#include "sdk.h"
#include "config.h"
#include "features.h"
#include "unlock.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdarg>

namespace Unlock {

    static void ULog(const char* fmt, ...)
    {
        char buf[1024]; va_list ap; va_start(ap, fmt);
        vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap); va_end(ap);
        Log("%s", buf);
    }

    enum Kind { K_NONE = 0, K_BOOL = 1, K_BYTE = 5, K_INT = 7, K_STR = 8 };
    enum WantKind { W_NONE = 0, W_TRUE, W_SKIN, W_RANK };

    struct Field { char name[64]; char type[96]; uint32_t off; Kind kind; WantKind want; };
    struct Holder { void* obj; Il2CppClass* cls; bool isArray; uint32_t off; std::vector<Field> fields; };

    static std::vector<Holder> g_holders;
    static void* g_arr = nullptr; static int g_arrLen = 0; static int g_reapply = 0; static bool g_installed = false;

    typedef Il2CppClass* (*fn_elem)(Il2CppClass*);
    typedef uint32_t(*fn_arrlen)(const Il2CppArray*);
    static fn_elem p_elem = nullptr; static fn_arrlen p_arrlen = nullptr;

    static void ResolveExtra()
    {
        if (p_elem && p_arrlen) return;
        HMODULE ga = GetModuleHandleA("GameAssembly.dll"); if (!ga) return;
        if (!p_elem)   p_elem = (fn_elem)GetProcAddress(ga, "il2cpp_class_get_element_class");
        if (!p_arrlen) p_arrlen = (fn_arrlen)GetProcAddress(ga, "il2cpp_array_length");
    }
    static int ArrLen(Il2CppArray* a) { if (!a) return 0; if (p_arrlen) return (int)p_arrlen(a); return *(int*)((uint8_t*)a + 0x18); }
    static void* ArrElem(Il2CppArray* a, int i) { if (!a || i < 0) return nullptr; return *(void**)((uint8_t*)a + 0x20 + (size_t)i * sizeof(void*)); }
    template<typename T> static void SW(void* p, uint32_t off, T v) { if (p) *(T*)((uint8_t*)p + off) = v; }

    static uint32_t SyncValOff(Il2CppClass* c) {
        if (!c) return 0xFFFFFFFFu; void* it = nullptr; FieldInfo* f;
        while ((f = il2cpp_class_get_fields(c, &it))) { const char* n = il2cpp_field_get_name(f); if (n && !strcmp(n, "_value")) return (uint32_t)il2cpp_field_get_offset(f); } return 0xFFFFFFFFu;
    }
    static void* SlotAddr(void* b, uint32_t off, const char* tn) {
        if (!b) return nullptr;
        if (tn && strstr(tn, "SyncVar<")) {
            void* in = *(void**)((uint8_t*)b + off); if (!in) return nullptr;
            uint32_t vo = SyncValOff(*(Il2CppClass**)in); if (vo == 0xFFFFFFFFu) return nullptr; return (uint8_t*)in + vo;
        }
        return (uint8_t*)b + off;
    }

    static Kind KindOf(const char* tn) {
        if (!tn) return K_NONE;
        if (strstr(tn, "Boolean")) return K_BOOL; if (strstr(tn, "Byte")) return K_BYTE;
        if (strstr(tn, "Int32")) return K_INT; if (strstr(tn, "String")) return K_STR; return K_NONE;
    }
    static WantKind WantOf(const char* n) {
        if (!n) return W_NONE;
        if (!strcmp(n, "available") || !strcmp(n, "unlocked") || !strcmp(n, "owned") || !strcmp(n, "purchased") || !strcmp(n, "bought")) return W_TRUE;
        if (!strcmp(n, "camoType") || !strcmp(n, "tracerType") || !strcmp(n, "mpCurrentCamoType")) return W_SKIN;
        if (!strcmp(n, "mpRank") || !strcmp(n, "rank") || !strcmp(n, "syncRank")) return W_RANK; return W_NONE;
    }

    static void ScanInto(Holder& h, Il2CppClass* cls)
    {
        if (!cls) return; void* it = nullptr; FieldInfo* f;
        while ((f = il2cpp_class_get_fields(cls, &it))) {
            const char* n = il2cpp_field_get_name(f); if (!n) continue;
            Il2CppType* t = il2cpp_field_get_type(f); const char* tn = t ? il2cpp_type_get_name(t) : nullptr;
            Kind k = KindOf(tn); if (k == K_NONE) continue; WantKind w = WantOf(n); if (w == W_NONE) continue;
            Field fd{}; strncpy_s(fd.name, n, _TRUNCATE); if (tn) strncpy_s(fd.type, tn, _TRUNCATE);
            fd.off = (uint32_t)il2cpp_field_get_offset(f); fd.kind = k; fd.want = w; h.fields.push_back(fd);
        }
    }

    static bool FindArray(void* p, uint32_t& outOff, Il2CppArray** out)
    {
        if (!p || !SDK::kPlayerScript || !SDK::kGunStats) return false;
        void* it = nullptr; FieldInfo* f; uint32_t bo = 0; Il2CppArray* ba = nullptr; bool bi = false;
        while ((f = il2cpp_class_get_fields(SDK::kPlayerScript, &it))) {
            Il2CppType* t = il2cpp_field_get_type(f); if (!t) continue;
            const char* tn = il2cpp_type_get_name(t); if (!tn || !strstr(tn, "[]")) continue;
            bool gun = false; Il2CppClass* tc = il2cpp_class_from_type(t);
            Il2CppClass* e = (p_elem && tc) ? p_elem(tc) : nullptr;
            if (e && e == SDK::kGunStats) gun = true; else if (strstr(tn, "gunStats[]")) gun = true;
            if (!gun) continue;
            uint32_t off = (uint32_t)il2cpp_field_get_offset(f); Il2CppArray* a = *(Il2CppArray**)((uint8_t*)p + off);
            if (ArrLen(a) <= 0) continue; bool inv = (off == 0xBA8);
            if (inv || !bi) { if (inv || !ba || ArrLen(a) > ArrLen(ba)) { bo = off; ba = a; bi = inv; } }
            if (inv) break;
        }
        if (!ba) return false; outOff = bo; *out = ba; return true;
    }

    // ---------- anexos ----------
    static void UnlockList(void* gun, uint32_t listOff)
    {
        if (!gun) return;
        void* list = *(void**)((uint8_t*)gun + listOff);
        if (!list) return;
        Il2CppArray* items = *(Il2CppArray**)((uint8_t*)list + 0x10);   // List._items
        int n = ArrLen(items);
        for (int i = 0; i < n; i++) {
            void* att = ArrElem(items, i);
            if (!att) continue;
            Il2CppClass* cls = *(Il2CppClass**)att;
            void* it = nullptr; FieldInfo* f;
            while ((f = il2cpp_class_get_fields(cls, &it))) {
                const char* fn = il2cpp_field_get_name(f); if (!fn) continue;
                Il2CppType* t = il2cpp_field_get_type(f); const char* tn = t ? il2cpp_type_get_name(t) : nullptr;
                if (tn && strstr(tn, "Boolean") && WantOf(fn) == W_TRUE)
                    SW<bool>(att, (uint32_t)il2cpp_field_get_offset(f), true);
            }
        }
    }

    static void ApplyField(void* base, const Field& f)
    {
        void* s = SlotAddr(base, f.off, f.type); if (!s) return;
        switch (f.want) {
        case W_TRUE: if (C.unlockAll) SW<bool>(s, 0, true); break;
        case W_SKIN: if (C.skinChanger) { if (f.kind == K_BYTE) SW<uint8_t>(s, 0, (uint8_t)C.skinIndex); else SW<int>(s, 0, C.skinIndex); } break;
        case W_RANK: if (f.kind == K_BYTE) SW<uint8_t>(s, 0, (uint8_t)(C.unlockLevel > 255 ? 255 : C.unlockLevel)); else SW<int>(s, 0, C.unlockLevel); break;
        default: break;
        }
    }

    bool Install()
    {
        ResolveExtra();
        void* p = SDK::g_localPlayer; if (!p) return false;
        g_holders.clear();
        Holder ph{}; ph.obj = p; ph.cls = SDK::kPlayerScript; ScanInto(ph, SDK::kPlayerScript);
        if (!ph.fields.empty()) g_holders.push_back(ph);
        uint32_t off = 0; Il2CppArray* a = nullptr;
        if (FindArray(p, off, &a)) {
            g_arr = a; g_arrLen = ArrLen(a);
            Holder ah{}; ah.cls = SDK::kGunStats; ah.isArray = true; ah.off = off; ScanInto(ah, SDK::kGunStats);
            if (!ah.fields.empty()) g_holders.push_back(ah);
        }
        g_installed = true;
        ULog("[unlock] install: holders=%d len=%d", (int)g_holders.size(), g_arrLen);
        return g_arr && g_arrLen > 0;
    }

    static void Refresh() {
        void* p = SDK::g_localPlayer; if (!p) return; uint32_t o = 0; Il2CppArray* a = nullptr;
        if (!FindArray(p, o, &a) || !a) return; int l = ArrLen(a); if (l <= 0) return; g_arr = a; g_arrLen = l;
    }

    void Tick()
    {
        if (!C.unlockAll && !C.skinChanger) return;
        if (!SDK::g_localPlayer) return;
        if (!g_installed || g_holders.empty() || !g_arr || g_arrLen <= 0) Install();
        Refresh();

        for (auto& h : g_holders) {
            if (!h.isArray) continue;
            for (int i = 0;i < g_arrLen;i++) {
                void* gun = ArrElem((Il2CppArray*)g_arr, i); if (!gun) continue;
                for (auto& f : h.fields) ApplyField(gun, f);
                if (C.unlockAttachments) { UnlockList(gun, 0x118); UnlockList(gun, 0x120); UnlockList(gun, 0x128); }
            }
        }
        for (auto& h : g_holders) { if (h.isArray || !h.obj) continue; for (auto& f : h.fields) ApplyField(h.obj, f); }

        if ((++g_reapply % 60) == 0) ULog("[unlock] reaplicado %d (holders=%d len=%d)", g_reapply, (int)g_holders.size(), g_arrLen);
    }

    void Dump() { ULog("=== DUMP UNLOCK: holders=%d len=%d ===", (int)g_holders.size(), g_arrLen); }

    // ---------- camos (usadas pelo menu) ----------
    static const char* kCamos[] = {
        "Nenhuma","Artico","Deserto","Floresta","Urbano","Digital","Neve",
        "Vermelho","Azul","Verde","Amarelo","Rosa","Ciano","Magenta",
        "Ouro","Prata","Bronze","Platina","Diamante","Rubi","Esmeralda","Safira",
        "Vermelho Tiger","Azul Tiger","Verde Tiger","Cinza Tiger","Roxo Tiger",
        "Fogo","Gelo","Toxic","Blood","Fantasma","Neon","Cyber","Vapor",
        "Camuflado","Carbono","Fibra","Titanio","Cromado","Ouro Negro",
        "Caveira","Caveira Ouro","Serpente","Dragao","Lobo","Falcao",
        "Estelar","Galaxia","Nebulosa","Aurora","Meteoro","Vulcao","Glacial",
        "Halloween","Natal","Aniversario","Lendario","Epico","Raro","Comum"
    };
    const char* const* CamoList() { return kCamos; }
    int CamoCount() { return (int)(sizeof(kCamos) / sizeof(kCamos[0])); }

} // namespace Unlock