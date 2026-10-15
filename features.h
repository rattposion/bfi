#pragma once

// ============================================================================
//  features.h — contrato de todos os modulos do cheat.
//  Nao define nada: so declara. As definicoes ficam nos .cpp de cada modulo.
//
//  Donos dos namespaces:
//    Menu::Render / Menu::ApplyStyle ....... features.cpp
//    Menu::Draw ............................ menu.cpp
//    ESP::Draw ............................. esp.cpp
//    Aimbot::Tick / Aimbot::Best ........... aimbot.cpp
//    Aimbot::Bone / Menu_WantsMouse ........ features.cpp
//    Triggerbot::* ......................... triggerbot.cpp
//    Weapons::Tick ......................... weapons.cpp
//    Combat::Tick .......................... combat.cpp
//    ConfigIO::* ........................... config.cpp
//    Misc::Draw ............................ visuals.cpp
//    Misc::Tick / Misc::Panic .............. misc.cpp
//    Movement::Tick ........................ movement.cpp
//    AntiCheat::* .......................... anticheat.cpp
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

// C vem de common.h como `Config& C` — NAO redeclarar
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
namespace Menu { void Render(); void Draw(); void ApplyStyle(); }
namespace ConfigIO { void Init(); void Save(); void Load(); }

bool Menu_WantsMouse();

// ---------------------------------------------------------------------------
//  opcoes de ESP compartilhadas entre o menu (features.cpp) e o desenho (esp.cpp)
//  O master e C.esp.
// ---------------------------------------------------------------------------
namespace Opts {
    extern bool  espBox;
    extern bool  espName;
    extern bool  espHealth;
    extern bool  espDistance;
    extern bool  espSnapline;
    extern float espMaxDist;
}

// ---------------------------------------------------------------------------
//  leitura/escrita tipada.
//  memory.h expoe SafeWrite(addr, ptr, size); estes helpers evitam
//  `SafeWrite<T>` (que nao existe) e tratam acesso a pagina protegida.
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

template <typename T>
inline bool ReadAtChecked(uintptr_t addr, T& out) {
    __try { memcpy(&out, (const void*)addr, sizeof(T)); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}