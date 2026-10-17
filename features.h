#pragma once

#include <atomic>
#include <cstdint>
#include "common.h"
#include "memory.h"
#include "sdk.h"
#include "config.h"

// ---- leitura / escrita crua ----
template<typename T> inline T    ReadAt(void* addr) { return addr ? *(T*)addr : (T)0; }
template<typename T> inline T    ReadAt(uintptr_t addr) { return *(T*)addr; }
template<typename T> inline T    ReadAt(void* base, uint32_t off) { return base ? *(T*)((uint8_t*)base + off) : (T)0; }
template<typename T> inline T    ReadAt(uintptr_t base, uint32_t off) { return *(T*)(base + off); }

template<typename T> inline void WriteAt(void* addr, T v) { if (addr) *(T*)addr = v; }
template<typename T> inline void WriteAt(uintptr_t addr, T v) { *(T*)addr = v; }
template<typename T> inline void WriteAt(void* base, uint32_t off, T v) { if (base) *(T*)((uint8_t*)base + off) = v; }
template<typename T> inline void WriteAt(uintptr_t base, uint32_t off, T v) { *(T*)(base + off) = v; }

// ---- estado global compartilhado ----
extern const char* g_phase;              // setado pelo PHASE()
extern std::atomic<bool> g_unload;

namespace Hooks {
    extern std::atomic<bool> g_imguiReady;
    extern std::atomic<bool> g_hooksLive;
    extern std::atomic<bool> g_sdkReady;
    extern int g_viewW;
    extern int g_viewH;
    bool Init();
    void Shutdown();
    void ApplyFeatures();
    void HandleKeys();
    bool InstallSwap();
    bool InstallMainThread();
    bool InstallCursor();
    bool InstallCombat();
}

namespace SDK { bool Init(); }

// Menu_WantsMouse: definida em features.cpp, usada pelo hook de input.
bool Menu_WantsMouse();

namespace Menu {
    extern bool g_open;
    void Draw();            // menu.cpp
    void Render();          // features.cpp — aplica o estilo 1x e chama Draw()
}

namespace ESP { void Draw(); }
namespace Misc { void Panic(); void Draw(); void Tick(); }

// ============================================================================
//  COMBAT + PROFILER POR PASSO
//  Cada passo do Tick e' cronometrado (QPC) e exposto pro overlay, pra voce
//  achar o gargalo real em vez de chutar.
// ============================================================================
namespace Combat {
    const int kMaxSteps = 16;
    struct StepStat { char name[24]; float ms; float last; };
    extern StepStat g_stats[kMaxSteps];
    extern int       g_statsN;
    extern float     g_tickMs;      // media exponencial do tick inteiro
    void Tick();
}

namespace Weapons { void Init(); void Tick(); }
namespace Movement { void Tick(); }

// ============================================================================
//  AIMBOT
//  NAO declare 'struct Quat' aqui dentro: cria Aimbot::Quat, um tipo distinto
//  do ::Quat do common.h, e toda chamada de SDK que recebe Quat& quebra
//  (E0312/E0349/E0434) alem de gerar LookRot sobrecarregado (E0308/C2668).
//  Regra: '::Quat' explicito aqui e no aimbot.cpp.
// ============================================================================
namespace Aimbot {
    Il2CppObject* Best();
    bool  Install();
    void  Tick();
    void  Shutdown();
    void  Bone(Il2CppObject* p, Il2CppObject* target);   // definida em features.cpp
    Vec3  Sub(Vec3, Vec3);
    float Dot(Vec3, Vec3);
    Vec3  Norm(Vec3);
    ::Quat LookRot(Vec3);
    ::Quat Slerp(::Quat, ::Quat, float);
    Vec3  AimPoint(void*);
    Vec3  LocalPos();
    Vec3  CamForward();
    namespace Silent { bool Override(Vec3 origin, Vec3& dir); }
}

namespace Recoil { void Install(); void Tick(); }

// ============================================================================
//  UNLOCK — API usada pelo menu (ver tambem unlock.h)
// ============================================================================
namespace Unlock {
    bool Install();
    void Tick();
    void Dump();

    // listas pro menu (ponteiros estaveis, montadas 1x)
    int WeaponCount();             const char* const* WeaponList();
    int AttachSlotCount();         const char* AttachSlotName(int slot);
    int AttachCount(int slot);     const char* const* AttachList(int slot);
    int AttachValue(int slot, int idx);
    int CamoCount();               const char* const* CamoList();
    int CharCount();               const char* const* CharList();
}

namespace Triggerbot { bool Install(); void Tick(); }
namespace AntiCheat { bool Install(); void Tick(); }