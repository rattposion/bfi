// aimbot.cpp - bfi
#include "common.h"
#include "sdk.h"
#include "config.h"
#include "features.h"
#include <cmath>

namespace Aimbot {

    static void* g_camTf = nullptr;
    static Il2CppObject* g_best = nullptr;

    Vec3 Sub(Vec3 a, Vec3 b) { return Vec3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
    float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vec3 Norm(Vec3 v) { float l = sqrtf(Dot(v, v)); if (l > 1e-6f) { v.x /= l; v.y /= l; v.z /= l; } return v; }

    Quat LookRot(Vec3 dir)
    {
        float yaw = atan2f(dir.x, dir.z);
        float pitch = -asinf(dir.y);
        float sy = sinf(yaw * 0.5f), cy = cosf(yaw * 0.5f);
        float sp = sinf(pitch * 0.5f), cp = cosf(pitch * 0.5f);
        return Quat{ cy * sp, sy * cp, -sy * sp, cy * cp };
    }
    Quat Slerp(Quat a, Quat b, float t)
    {
        float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        if (d < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
        if (d > 0.9995f) return b;
        float th = acosf(d), s = sinf(th);
        float wa = sinf((1 - t) * th) / s, wb = sinf(t * th) / s;
        return Quat{ a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb };
    }

    Vec3 AimPoint(void* obj)
    {
        for (auto& t : SDK::g_targets)
            if (t.obj == obj) {
                if (C.aimBone <= 1) return t.head;
                Vec3 p = t.head;
                p.y -= (t.head.y - t.feet.y) * 0.18f;
                return p;
            }
        return Vec3{};
    }
    Vec3 LocalPos()
    {
        for (auto& t : SDK::g_targets)
            if (t.obj == SDK::g_localPlayer) return t.head;
        return Vec3{};
    }
    Vec3 CamForward()
    {
        void* p = SDK::g_localPlayer;
        if (!p) return Vec3{ 0, 0, 1 };
        Quat q = *(Quat*)((uint8_t*)p + 0x9B8);
        return Aimbot::Norm(Vec3{ 2.f * (q.x * q.z + q.w * q.y), 2.f * (q.y * q.z - q.w * q.x), 1.f - 2.f * (q.x * q.x + q.y * q.y) });
    }

    static bool KeyDown() { return (GetAsyncKeyState(C.aimKey) & 0x8000) != 0; }

    Il2CppObject* Best()
    {
        g_best = nullptr;
        if (!C.aimbot || !SDK::g_localPlayer) return nullptr;

        void* local = SDK::g_localPlayer;
        Vec3 fwd = Aimbot::CamForward();

        float bestAngle = C.aimFov;
        for (auto& t : SDK::g_targets) {
            if (!t.obj || t.obj == local) continue;
            if (C.aimTeam && t.team == *(uint8_t*)((uint8_t*)local + 0x6D0)) continue;
            Vec3 dir = Aimbot::Norm(Aimbot::Sub(Aimbot::AimPoint(t.obj), Aimbot::LocalPos()));
            float ang = acosf(Aimbot::Dot(fwd, dir)) * (180.f / 3.14159265f);
            if (ang < bestAngle) { bestAngle = ang; g_best = (Il2CppObject*)t.obj; }
        }
        return g_best;
    }

    bool Install()
    {
        void* p = SDK::g_localPlayer;
        if (p) g_camTf = *(void**)((uint8_t*)p + 0x828);
        Log("[aimbot] install: fov=%.1f tecla=0x%02X", C.aimFov, C.aimKey);
        return true;
    }

    void Tick()
    {
        if (!C.aimbot || !SDK::g_localPlayer) return;
        Il2CppObject* best = Best();
        if (!best) return;
        if (!KeyDown()) return;

        void* local = SDK::g_localPlayer;
        Vec3 dir = Aimbot::Norm(Aimbot::Sub(Aimbot::AimPoint((void*)best), Aimbot::LocalPos()));
        Quat want = Aimbot::LookRot(dir);
        Quat cur = *(Quat*)((uint8_t*)local + 0x9B8);
        float t = (C.aimSmooth <= 0.f) ? 1.f : (1.f / C.aimSmooth);
        *(Quat*)((uint8_t*)local + 0x9B8) = Aimbot::Slerp(cur, want, t);
    }

    namespace Silent {
        bool Override(Vec3 origin, Vec3& dir)
        {
            if (!C.silentAim && !C.magicBullets) return false;
            void* local = SDK::g_localPlayer;
            if (!local) return false;

            float best = 180.f; void* bestT = nullptr;
            for (auto& t : SDK::g_targets) {
                if (!t.obj || t.obj == local) continue;
                if (C.aimTeam && t.team == *(uint8_t*)((uint8_t*)local + 0x6D0)) continue;
                Vec3 d = Aimbot::Norm(Aimbot::Sub(Aimbot::AimPoint(t.obj), origin));
                float ang = acosf(Aimbot::Dot(dir, d)) * (180.f / 3.14159265f);
                if (ang < best) { best = ang; bestT = t.obj; }
            }
            if (!bestT) return false;

            dir = Aimbot::Norm(Aimbot::Sub(Aimbot::AimPoint(bestT), origin));
            return true;
        }
    }

} // namespace Aimbot