#pragma once
#include "il2cpp.h"

template<typename T> inline T SafeRead(uintptr_t a) {
    if (!a) return T{};
    __try { return *(T*)a; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
}
template<typename T> inline void SafeWrite(uintptr_t a, const T& v) {
    if (!a) return;
    __try { *(T*)a = v; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
inline bool Valid(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; }

#define FIELD(cls, obj, off, T) (*(T*)((uintptr_t)(obj) + (off)))