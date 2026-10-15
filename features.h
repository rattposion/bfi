#pragma once

// ============================================================================
//  features.h — contrato de todos os modulos do cheat.
// ============================================================================
#include "common.h"
#include "il2cpp.h"
#include "il2cpp_api.h"
#include "memory.h"
#include "sdk.h"
#include "config.h"

#include <atomic>
#include <cstring>
#include <windows.h>

extern Config g_cfg;
extern std::atomic<bool> g_unload;

void Log(const char* fmt, ...);

// ---------------------------------------------------------------------------
//  modulos
// ---------------------------------------------------------------------------
namespace Combat { void Tick(); }

namespace Aimbot {
    void Tick(); Il2CppObject* Best();
    void Bone(Il2CppObject* p, Il2CppObject* target);
    bool Install();
}

namespace Triggerbot { void Tick(); bool Install(); }
namespace Weapons { void Tick(); }
namespace Movement { void Tick(); }

namespace Misc { void Tick(); void Draw(); void Panic(); }
namespace ESP { void Draw(); }
namespace AntiCheat { bool Install(); void Tick(); }

// Menu::Draw  -> menu.cpp
// Menu::Render/ApplyStyle -> features.cpp
namespace Menu { void Render(); void Draw(); void ApplyStyle(); }

namespace ConfigIO { void Init(); void Save(); void Load(); }

bool Menu_WantsMouse();

// ---------------------------------------------------------------------------
//  leitura/escrita tipada.
// ---------------------------------------------------------------------------
template <typename T>
inline T ReadAt(uintptr_t addr, T def = T{}) {
    T v = def;
    __try { memcpy(&v, (const void*)addr, sizeof(T)); }
    __except (EXCEPTION_EXECUTE_HANDLER) { v = def; }
    return v;
}

template <typename T>
inline void WriteAt(uintptr_t addr, const T& v) {
    SafeWrite(addr, (const void*)&v, sizeof(T));
}