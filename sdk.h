#pragma once
#include "memory.h"

// ATENCAO: Quat ja vem de common.h. NAO redefinir aqui.

struct Target {
    Il2CppObject* player = nullptr;
    Il2CppObject* headBone = nullptr;
    Il2CppObject* footBone = nullptr;
    void* klass = nullptr;
    Vec3   head, feet;
    Vec2   scrHead, scrFeet;      // projetado na MAIN THREAD
    bool   onScreen = false;
    float  dist = 0.f;
    int    health = 100;
    float  hp = 100.f;
    float  hpMax = 100.f;
    bool   alive = true;
    int    team = -1;
    std::string name;
    std::string weapon;
    uint64_t seen = 0;
    uint64_t boneTry = 0;        // ultima tentativa de achar bones
};

extern Target g_targets[64];
extern int g_targetCount;
extern CRITICAL_SECTION g_targetCs;

extern Il2CppObject* g_localPlayer;
extern Il2CppObject* g_camera;
extern int           g_localTeam;
extern Vec3          g_localPos;
extern bool          g_inGame;
extern const char* g_phase;     // <- diagnostico de crash

void AddTarget(Il2CppObject* p, bool trust = false);
void PruneTargets();
void ScanAllTargets();
void RefreshTargets();

namespace SDK {
    bool Init();

    extern Il2CppClass* kEntity, * kPlayerScript, * kPlayer, * kNPC;
    extern Il2CppClass* kTransform, * kComponent, * kCamera, * kScreen, * kCursor, * kPhysics, * kAntiHack, * kWeapon;

    extern size_t O_health, O_healthValue, O_team, O_rotation, O_camTf, O_camField, O_alive, O_gunView, O_gunStats, O_name;
    extern size_t O_curAmmo, O_maxAmmo, O_reloading, O_spread, O_fireRate, O_range, O_speed, O_vel, O_gunStatsOnView;
    extern bool   O_healthIsFloat;
    extern bool   O_rotationIsQuat;

    extern FieldInfo* F_gmInstance;
    extern const MethodInfo* M_getPlayerScript;
    extern const MethodInfo* M_get_instance;
    extern const MethodInfo* M_get_transform, * M_get_position, * M_set_position;
    extern const MethodInfo* M_get_childCount, * M_GetChild, * M_get_name, * M_get_rotation, * M_set_rotation;
    extern const MethodInfo* M_get_main, * M_get_current, * M_get_allCameras;
    extern const MethodInfo* M_WorldToScreen, * M_get_proj, * M_get_view, * M_get_pw, * M_get_ph, * M_get_fov, * M_set_fov;
    extern const MethodInfo* M_GetComponentType;
    extern const MethodInfo* M_get_width, * M_get_height;
    extern const MethodInfo* M_set_lockState, * M_set_visible;
    extern const MethodInfo* M_FireBullet, * M_ComputeRecoil, * M_hurt, * M_localHurt, * M_get_team, * M_LateUpdate, * M_shoot;
    extern const MethodInfo* M_get_forward, * M_set_forward, * M_LookAt, * M_Raycast3, * M_Raycast4, * M_Linecast, * M_hitDist;
    extern const MethodInfo* M_getHealth, * M_getMaxHealth, * M_getDisplayName;
    extern const MethodInfo* M_FindObjects, * M_FindObjectsAll;

    float ScreenW();
    float ScreenH();

    Il2CppClass* FindClass(const char* name, const char* ns = "");
    const char* SafeClassName(Il2CppClass* k);
    void* SafeReadObjPub(void* o, size_t off);

    bool IsA(Il2CppObject* o, Il2CppClass* c);
    Il2CppObject* GameManager();
    Il2CppObject* LocalPlayer();
    Il2CppObject* MainCamera();
    Il2CppObject* CameraObj();
    Il2CppObject* GetTransform(Il2CppObject* o);
    Il2CppObject* GetComponentType(Il2CppObject* o, Il2CppClass* klass);
    Vec3 GetPos(Il2CppObject* t);
    Vec3 GetForward(Il2CppObject* t);
    Quat GetRot(Il2CppObject* t);
    bool SetRot(Il2CppObject* t, const Quat& q);

    float InvokeFloat(const MethodInfo* m, Il2CppObject* o, float fb);
    int   GetHealth(Il2CppObject* p);
    float GetHealthPct(Il2CppObject* p);
    bool  ReadAliveField(Il2CppObject* p, bool fb);
    bool  IsAlive(Il2CppObject* p);

    int         GetTeam(Il2CppObject* p);
    void        GetNameBuf(Il2CppObject* p, char* out, int outSize);
    std::string GetName(Il2CppObject* p);
    Il2CppObject* FindBoneContains(Il2CppObject* t, const char* needle, int depth, int* budget);   // NOVO
    bool WorldToScreen(const Vec3& w, Vec2& out);
    bool VisibleLine(const Vec3& from, const Vec3& to);
    Il2CppObject* LocalGun();
    Vec3 LocalPos();

    extern int dbg_scanCount, dbg_reject, dbg_adds, dbg_drawn;
}