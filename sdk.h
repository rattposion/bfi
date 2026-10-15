#pragma once
#include "memory.h"

struct Quat { float x, y, z, w; };

struct Target {
    Il2CppObject* player = nullptr;
    Il2CppObject* headBone = nullptr;
    Il2CppObject* footBone = nullptr;
    void* klass = nullptr;
    Vec3 head, feet;
    float dist = 0.f;
    int   health = 100;
    float hp = 100.f;
    float hpMax = 100.f;
    bool  alive = true;
    int   team = -1;
    std::string name;
    uint64_t seen = 0;
};

extern Target g_targets[64];
extern int g_targetCount;
extern CRITICAL_SECTION g_targetCs;
void AddTarget(Il2CppObject* p, bool trust = false);
void PruneTargets();

namespace SDK {
    bool Init();

    extern Il2CppClass* kPlayer, * kTransform, * kComponent, * kCamera, * kScreen, * kCursor, * kPhysics, * kAntiHack, * kWeapon;

    extern size_t O_health, O_healthValue, O_team, O_rotation, O_camTf, O_camField, O_alive, O_gunView, O_gunStats, O_name;
    extern size_t O_curAmmo, O_maxAmmo, O_reloading, O_spread, O_fireRate, O_range;
    extern size_t O_speed, O_vel;

    extern void* F_gmInstance;
    extern const MethodInfo* M_getPlayerScript;
    extern const MethodInfo* M_get_instance;
    extern const MethodInfo* M_get_transform, * M_get_position, * M_set_position;
    extern const MethodInfo* M_get_childCount, * M_GetChild, * M_get_name, * M_get_rotation;
    extern const MethodInfo* M_get_main, * M_get_current, * M_get_allCameras;
    extern const MethodInfo* M_WorldToScreen, * M_get_proj, * M_get_view, * M_get_pw, * M_get_ph, * M_get_fov;
    extern const MethodInfo* M_GetComponentType;
    extern const MethodInfo* M_get_width, * M_get_height;
    extern const MethodInfo* M_set_lockState, * M_set_visible;
    extern const MethodInfo* M_FireBullet, * M_ComputeRecoil, * M_hurt, * M_localHurt, * M_get_team, * M_LateUpdate, * M_shoot;
    extern const MethodInfo* M_get_forward, * M_set_forward, * M_LookAt, * M_Raycast3, * M_Raycast4, * M_Linecast, * M_hitDist;
    extern const MethodInfo* M_getHealth, * M_getMaxHealth;

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
    Il2CppObject* FindFirstCamera();
    Il2CppObject* GetTransform(Il2CppObject* o);
    Il2CppObject* GetComponentType(Il2CppObject* o, Il2CppClass* klass);
    Vec3 GetPos(Il2CppObject* t);
    Vec3 GetForward(Il2CppObject* t);
    Quat GetRot(Il2CppObject* t);

    float InvokeFloat(const MethodInfo* m, Il2CppObject* o, float fb);
    int   GetHealth(Il2CppObject* p);
    float GetHealthPct(Il2CppObject* p);
    bool  ReadAliveField(Il2CppObject* p, bool fb);
    bool  IsAlive(Il2CppObject* p);

    int GetTeam(Il2CppObject* p);
    std::string GetName(Il2CppObject* p);
    Il2CppObject* FindBone(Il2CppObject* t, const char* needle, int depth = 0);
    Il2CppObject* FindBoneExato(Il2CppObject* t, const char* nome, int depth, Il2CppObject** loose);
    bool WorldToScreen(const Vec3& w, Vec2& out);
    bool Visible(const Vec3& from, const Vec3& to);
    bool VisibleLine(const Vec3& from, const Vec3& to);
    Il2CppObject* LocalGun();
    Il2CppObject* CamTfRaw();
    Il2CppObject* CamTransform();
    Vec3 LocalPos();
    void ScanPlayers();

    extern const MethodInfo* M_FindObjects, * M_FindObjectsAll;
    extern int dbg_scanCount, dbg_ok_late, dbg_ok_w2s, dbg_ok_find, dbg_ok_look, dbg_ok_camTf, dbg_ok_cam, dbg_drawn, dbg_teamSkip, dbg_reject, dbg_adds;
    extern Il2CppObject* dbg_local;
}