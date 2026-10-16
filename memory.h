#pragma once

#include "common.h"
#include <psapi.h>

// =====================================================================
//  memoria segura (nunca estoura o processo)
// =====================================================================
inline bool IsBadPtr(uintptr_t a) {
    return a < 0x10000 || a > 0x7FFFFFFFFFFFULL;
}

template <typename T>
inline T SafeRead(uintptr_t addr, T fallback) {
    if (IsBadPtr(addr)) return fallback;
    __try { return *(T*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return fallback; }
}

template <typename T>
inline T SafeRead(uintptr_t addr) {
    return SafeRead<T>(addr, T{});
}

template <typename T>
inline void SafeWrite(uintptr_t addr, const T& val) {
    if (IsBadPtr(addr)) return;
    __try { *(T*)addr = val; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

template <typename T>
inline void SafeWrite(void* addr, const T& val) {
    SafeWrite<T>((uintptr_t)addr, val);
}

inline void SafeWrite(uintptr_t addr, const void* src, size_t n) {
    if (IsBadPtr(addr) || !src) return;
    __try { memcpy((void*)addr, src, n); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// =====================================================================
//  modulos / pattern scan
// =====================================================================
inline uintptr_t ModuleBase(const char* name) {
    return (uintptr_t)GetModuleHandleA(name);
}

inline size_t ModuleSize(const char* name) {
    HMODULE h = GetModuleHandleA(name);
    if (!h) return 0;
    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) return 0;
    return mi.SizeOfImage;
}

inline uintptr_t PatternScan(const char* mod, const char* pat, const char* mask) {
    uintptr_t base = ModuleBase(mod);
    size_t    size = ModuleSize(mod);
    if (!base || !size) return 0;

    size_t plen = strlen(mask);
    if (!plen) return 0;

    for (size_t i = 0; i + plen <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < plen; ++j) {
            if (mask[j] == 'x' && *(unsigned char*)(base + i + j) != (unsigned char)pat[j]) {
                ok = false;
                break;
            }
        }
        if (ok) return base + i;
    }
    return 0;
}

inline uintptr_t Scan(const char* mod, const char* sig) {
    // sig no formato "48 8B 05 ?? ?? ?? ?? 48 85 C0"
    unsigned char bytes[128] = {};
    char          mask[128] = {};
    size_t        n = 0;

    const char* p = sig;
    while (*p && n < 127) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?') {
            bytes[n] = 0;
            mask[n] = '?';
            ++n;
            while (*p && *p != ' ') ++p;
        }
        else {
            bytes[n] = (unsigned char)strtoul(p, nullptr, 16);
            mask[n] = 'x';
            ++n;
            while (*p && *p != ' ') ++p;
        }
    }
    return PatternScan(mod, (const char*)bytes, mask);
}

// =====================================================================
//  leitura/escrita crua
// =====================================================================
inline bool ReadMem(uintptr_t addr, void* out, size_t n) {
    if (IsBadPtr(addr) || !out) return false;
    __try { memcpy(out, (void*)addr, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool WriteMem(uintptr_t addr, const void* in, size_t n) {
    if (IsBadPtr(addr) || !in) return false;
    DWORD old = 0;
    if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old)) return false;

    bool ok = true;
    __try { memcpy((void*)addr, in, n); }
    // >>> FIX: restaura a protecao ANTES de sair, inclusive no caminho de excecao
    //     (antes, a pagina ficava PAGE_EXECUTE_READWRITE para sempre).
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }

    DWORD tmp = 0;
    VirtualProtect((void*)addr, n, old, &tmp);
    return ok;
}