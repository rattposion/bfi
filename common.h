#pragma once

// =====================================================================
//  Config global. Funciona das DUAS formas apontando para o MESMO objeto:
//      C.esp   -> variavel (referencia)
//      C(esp)  -> macro
// =====================================================================
struct Config;
extern Config  g_cfg;
extern Config& C;

#ifdef C
#undef C
#endif
#define C(f) g_cfg.f

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <initializer_list>

#include "imgui.h"
#include "MinHook.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern HMODULE g_hModule;

struct Vec2 { float x, y; };
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float len()  const { return sqrtf(x * x + y * y + z * z); }
    float dist(const Vec3& o) const { return (*this - o).len(); }
    bool  zero() const { return x == 0.f && y == 0.f && z == 0.f; }
    // >>> FIX: (0,0,0) e um ponto VALIDO. zero() nao serve para detectar falha
    //     de resolucao em TargetAimPos. isFinite() serve.
    bool  isFinite() const {
        return x == x && y == y && z == z &&
            fabsf(x) < 1.0e6f && fabsf(y) < 1.0e6f && fabsf(z) < 1.0e6f;
    }
};
struct Quat { float x, y, z, w; };

void LogInit();
void Log(const char* fmt, ...);