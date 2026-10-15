#pragma once

#include "common.h"
#include "il2cpp.h"
#include "il2cpp_api.h"
#include "memory.h"
#include "config.h"

#include <vector>
#include <string>
#include <windows.h>

namespace Hooks {
    extern int g_viewW;
    extern int g_viewH;
}

extern const char* g_phase;
#ifndef PHASE
#define PHASE(x) (g_phase = (x))
#endif

extern Config g_cfg;

#ifndef C
#define C(f) #f, &g_cfg.f
#endif

#define MAX_TARGETS 128

struct Target {
    Il2CppObject* obj = nullptr;
    Il2CppObject* player = nullptr;

    Il2CppObject* headBone = nullptr;
    Il2CppObject* footBone = nullptr;

    Vec3   pos{ 0.f, 0.f, 0.f };
    Vec3   feet{ 0.f, 0.f, 0.f };
    Vec3   head{ 0.f, 0.f, 0.f };
    Vec2   scrHead{ 0.f, 0.f };
    Vec2   scrFeet{ 0.f, 0.f };

    bool   onScreen = false;
    bool   alive = false;
    bool   enemy = true;
    bool   npc = false;
    bool   isLocal = false;

    int    team = 0;
    int    weapon = 0;
    float  health = 100.f;
    float  hp = 100.f;
    float  dist = 0.f;

    uint64_t lastSeen = 0;
    uint64_t lastBoneScan = 0;

    char   name[40]{};
};

namespace SDK {

    // ---- offsets (linkage externo) ----
    extern uint32_t O_health, O_alive, O_team, O_isLocal, O_name, O_weapon;
    extern uint32_t O_vel, O_speed, O_rot, O_rotation, O_cam, O_camTf;
    extern uint32_t O_mCamera, O_humanMeshAnimator, O_humanMeshReferences;
    extern uint32_t O_stats, O_gunView, O_gunStats, O_gunStatsOnView;
    extern uint32_t O_curAmmo, O_maxAmmo, O_reloading, O_spread, O_fireRate, O_dmg;
    extern bool     O_rotationIsQuat;

    // ---- classes ----
    extern Il2CppClass* kEntity;
    extern Il2CppClass* kPlayerScript;
    extern Il2CppClass* kPlayer;
    extern Il2CppClass* kNPC;
    extern Il2CppClass* kTransform;
    extern Il2CppClass* kCamera;
    extern Il2CppClass* kGunStats;
    extern Il2CppClass* kVector3;
    extern Il2CppClass* kQuaternion;
    extern Il2CppClass* kAntiHack;

    // ---- metodos ----
    extern MethodInfo* M_shoot;
    extern MethodInfo* M_recoil;
    extern MethodInfo* M_fireBullet;
    extern MethodInfo* M_FireBullet;
    extern MethodInfo* M_getTransform;
    extern MethodInfo* M_w2s;
    extern MethodInfo* M_lookRot;
    extern MethodInfo* M_set_position;
    extern MethodInfo* M_set_forward;
    extern MethodInfo* M_get_fov;
    extern MethodInfo* M_set_fov;

    // ---- estado ----
    extern Il2CppObject* g_camera;
    extern Il2CppObject* g_local;
    extern Il2CppObject* g_aimTarget;
    extern Il2CppObject* g_localPlayer;
    extern int   g_localTeam;
    extern Vec3  g_localPos;
    extern Target g_targets[MAX_TARGETS];
    extern int   g_targetCount;
    extern CRITICAL_SECTION g_targetCs;
    extern int   dbg_drawn;
    extern uint64_t g_frameId;

    extern bool  g_viewProjOk;
    extern float g_viewProj[16];

    // ---- api ----
    int           ScreenW();
    int           ScreenH();
    Il2CppObject* CameraObj();

    bool Init();
    void Shutdown();

    void UpdateView();
    void RefreshTargets();
    void UpdateTargets();
    std::vector<Target> Snapshot();

    Il2CppObject* GetLocalPlayer();
    Il2CppObject* GetCamera();
    bool  IsA(Il2CppObject* o, Il2CppClass* c);
    bool  IsEnemy(Il2CppObject* o);
    float GetHealth(Il2CppObject* o);
    float GetHealth(void* o);
    Vec3  GetPos(Il2CppObject* o);
    Vec3  GetPos(void* o);
    void  SetPos(Il2CppObject* o, Vec3 v);
    void  SetPos(void* o, Vec3 v);
    Vec3  GetVelocity(Il2CppObject* o);

    Il2CppObject* FindBoneContains(Il2CppObject* root, const char* sub, int budget = 8192);
    Vec3  GetBonePos(void* bone);
    void  GetNameBuf(Il2CppObject* o, char* out, int n);
    std::string GetName(Il2CppObject* o);

    bool  WorldToScreen(Vec3 w, Vec2& s);
    bool  VisibleLine(Vec3 a, Vec3 b);

    Il2CppObject* GetGunStats(Il2CppObject* player);
    Il2CppObject* GetWeapon(Il2CppObject* player);

    bool  AimLock(Il2CppObject* p, Quat& savedOriginal);
    void  RestoreAim(Il2CppObject* p, const Quat& q);
    void  FireBulletAt(Vec3 dir);

    void  DumpFields(const char* className);
    void  DumpFields(Il2CppClass* k, const char* tag);
    void  LogHierarchy(Il2CppObject* root, int maxNodes);

    Il2CppClass* FindClass(const char* name, const char* ns = nullptr);
    Il2CppObject* GetTransform(void* obj);
    Il2CppObject* SafeReadObjPub(void* obj, uint32_t off);

    void  LockTargets();
    void  UnlockTargets();

    void* MethodPtr(MethodInfo* m);
}

// ---- atalhos no escopo global ----
using SDK::g_targets;
using SDK::g_targetCount;
using SDK::g_targetCs;
using SDK::g_localPlayer;
using SDK::g_localTeam;
using SDK::g_localPos;
using SDK::dbg_drawn;
using SDK::ScreenW;
using SDK::ScreenH;
using SDK::CameraObj;
using SDK::FindClass;
using SDK::GetTransform;
using SDK::SafeReadObjPub;
using SDK::VisibleLine;
using SDK::LockTargets;
using SDK::UnlockTargets;
using SDK::FindBoneContains;
using SDK::GetBonePos;
using SDK::GetPos;
using SDK::SetPos;
using SDK::GetHealth;
using SDK::IsA;
using SDK::IsEnemy;
using SDK::GetName;
using SDK::GetNameBuf;
using SDK::GetVelocity;
using SDK::GetGunStats;
using SDK::GetWeapon;
using SDK::GetLocalPlayer;
using SDK::DumpFields;

using SDK::O_health;
using SDK::O_alive;
using SDK::O_team;
using SDK::O_vel;
using SDK::O_speed;
using SDK::O_rot;
using SDK::O_stats;
using SDK::O_gunView;
using SDK::O_cam;
using SDK::O_name;
using SDK::O_weapon;
using SDK::O_isLocal;
using SDK::O_dmg;
using SDK::O_gunStats;
using SDK::O_gunStatsOnView;
using SDK::O_curAmmo;
using SDK::O_maxAmmo;
using SDK::O_reloading;
using SDK::O_spread;
using SDK::O_fireRate;
using SDK::O_rotation;
using SDK::O_camTf;
using SDK::O_mCamera;
using SDK::O_humanMeshAnimator;
using SDK::O_humanMeshReferences;
using SDK::O_rotationIsQuat;

using SDK::M_shoot;
using SDK::M_recoil;
using SDK::M_fireBullet;
using SDK::M_FireBullet;
using SDK::M_set_position;
using SDK::M_set_forward;
using SDK::M_get_fov;
using SDK::M_set_fov;
using SDK::M_w2s;
using SDK::M_getTransform;
using SDK::M_lookRot;

using SDK::kEntity;
using SDK::kPlayerScript;
using SDK::kPlayer;
using SDK::kNPC;
using SDK::kTransform;
using SDK::kCamera;
using SDK::kGunStats;
using SDK::kVector3;
using SDK::kQuaternion;
using SDK::kAntiHack;

using SDK::g_camera;
using SDK::g_local;
using SDK::g_aimTarget;