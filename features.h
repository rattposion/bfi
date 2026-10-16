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
namespace Menu { extern bool g_open; void Draw(); void Render(); }
namespace ESP { void Draw(); }
namespace Misc { void Panic(); void Draw(); void Tick(); }
namespace Combat { void Tick(); }
namespace Weapons { void Init(); void Tick(); }
namespace Movement { void Tick(); }

namespace Aimbot {
    struct Quat { float x, y, z, w; };
    Il2CppObject* Best();
    bool  Install();
    void  Tick();
    Vec3  Sub(Vec3, Vec3);
    float Dot(Vec3, Vec3);
    Vec3  Norm(Vec3);
    Quat  LookRot(Vec3);
    Quat  Slerp(Quat, Quat, float);
    Vec3  AimPoint(void*);
    Vec3  LocalPos();
    Vec3  CamForward();
    namespace Silent { bool Override(Vec3 origin, Vec3& dir); }
}

namespace Recoil { void Install(); void Tick(); }
namespace Unlock { bool Install(); void Tick(); void Dump(); }
namespace Triggerbot { bool Install(); void Tick(); }
namespace AntiCheat { bool Install(); void Tick(); }