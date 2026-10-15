#include "sdk.h"
#include "config.h"

Target g_targets[64];
int    g_targetCount = 0;
CRITICAL_SECTION g_targetCs;

Il2CppObject* g_localPlayer = nullptr;
Il2CppObject* g_camera = nullptr;
int           g_localTeam = -1;
Vec3          g_localPos;
bool          g_inGame = false;

namespace SDK {
    Il2CppClass* kEntity, * kPlayerScript, * kPlayer, * kNPC;
    Il2CppClass* kTransform, * kComponent, * kCamera, * kScreen, * kCursor, * kPhysics, * kAntiHack, * kWeapon;

    size_t O_health, O_healthValue, O_team, O_rotation, O_camTf, O_camField, O_alive, O_gunView, O_gunStats, O_name;
    size_t O_curAmmo, O_maxAmmo, O_reloading, O_spread, O_fireRate, O_range, O_speed, O_vel, O_gunStatsOnView;
    bool   O_healthIsFloat = false;
    bool   O_rotationIsQuat = false;

    FieldInfo* F_gmInstance = nullptr;
    const MethodInfo* M_getPlayerScript = nullptr;
    const MethodInfo* M_get_instance = nullptr;
    const MethodInfo* M_get_transform = nullptr, * M_get_position = nullptr, * M_set_position = nullptr;
    const MethodInfo* M_get_childCount = nullptr, * M_GetChild = nullptr, * M_get_name = nullptr;
    const MethodInfo* M_get_rotation = nullptr, * M_set_rotation = nullptr;
    const MethodInfo* M_get_main = nullptr, * M_get_current = nullptr, * M_get_allCameras = nullptr;
    const MethodInfo* M_WorldToScreen = nullptr, * M_get_proj = nullptr, * M_get_view = nullptr;
    const MethodInfo* M_get_pw = nullptr, * M_get_ph = nullptr, * M_get_fov = nullptr, * M_set_fov = nullptr;
    const MethodInfo* M_GetComponentType = nullptr;
    const MethodInfo* M_get_width = nullptr, * M_get_height = nullptr;
    const MethodInfo* M_set_lockState = nullptr, * M_set_visible = nullptr;
    const MethodInfo* M_FireBullet = nullptr, * M_ComputeRecoil = nullptr, * M_hurt = nullptr;
    const MethodInfo* M_localHurt = nullptr, * M_get_team = nullptr, * M_LateUpdate = nullptr, * M_shoot = nullptr;
    const MethodInfo* M_get_forward = nullptr, * M_set_forward = nullptr, * M_LookAt = nullptr;
    const MethodInfo* M_Raycast3 = nullptr, * M_Raycast4 = nullptr, * M_Linecast = nullptr, * M_hitDist = nullptr;
    const MethodInfo* M_getHealth = nullptr, * M_getMaxHealth = nullptr, * M_getDisplayName = nullptr;
    const MethodInfo* M_FindObjects = nullptr, * M_FindObjectsAll = nullptr;

    int dbg_scanCount = 0, dbg_reject = 0, dbg_adds = 0, dbg_drawn = 0;

    static Il2CppImage* g_imgs[512];
    static int  g_imgCount = 0;
    static float g_sw = 0.f, g_sh = 0.f;

    float ScreenW() { return g_sw; }
    float ScreenH() { return g_sh; }

    // ------------------------------------------------------------------ helpers
    static Il2CppObject* SafeInvoke(const MethodInfo* m, void* o, void** args) {
        if (!m) return nullptr;
        __try { return IL2CPP::Invoke(m, o, args); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static void* SafeUnbox(Il2CppObject* o) {
        if (!o) return nullptr;
        __try { return IL2CPP::object_unbox(o); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static bool SafeReadBool(void* o, size_t off, bool fb) {
        if (!o || !off) return fb;
        __try { return *(bool*)((uintptr_t)o + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return fb; }
    }
    static int SafeReadInt(void* o, size_t off, int fb) {
        if (!o || !off) return fb;
        __try { return *(int*)((uintptr_t)o + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return fb; }
    }
    static float SafeReadFloat(void* o, size_t off, float fb) {
        if (!o || !off) return fb;
        __try { return *(float*)((uintptr_t)o + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return fb; }
    }
    static Il2CppObject* SafeStaticGet(FieldInfo* field) {
        if (!field) return nullptr;
        void* v = nullptr;
        __try { IL2CPP::field_static_get_value(field, &v); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return (Il2CppObject*)v;
    }
    static Il2CppClass* SafeClassFromType(const Il2CppType* t) {
        if (!t || !IL2CPP::class_from_type) return nullptr;
        __try { return IL2CPP::class_from_type(t); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static const char* SafeTypeName(const Il2CppType* t) {
        if (!t) return nullptr;
        __try { return IL2CPP::type_get_name(t); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static const MethodInfo* PickRaycast4(Il2CppClass* k) {
        if (!k) return nullptr;
        void* it = nullptr;
        while (const MethodInfo* m = IL2CPP::class_get_methods(k, &it)) {
            const char* nm = IL2CPP::method_get_name(m);
            if (!nm || lstrcmpA(nm, "Raycast") != 0) continue;
            if (IL2CPP::method_get_param_count(m) != 4) continue;
            const char* rtn = IL2CPP::method_get_return_type
                ? SafeTypeName(IL2CPP::method_get_return_type(m)) : nullptr;
            if (!rtn || !strstr(rtn, "Boolean")) continue;
            const char* tn = SafeTypeName(IL2CPP::method_get_param(m, 2));
            if (!tn || !strstr(tn, "RaycastHit")) continue;
            const char* pd = SafeTypeName(IL2CPP::method_get_param(m, 3));
            if (!pd || !strstr(pd, "Single")) continue;
            return m;
        }
        return nullptr;
    }

    static void CollectImages() {
        size_t n = 0;
        Il2CppAssembly** as = IL2CPP::domain_get_assemblies(IL2CPP::domain_get(), &n);
        if (!as) return;
        if (n > 512) n = 512;
        for (size_t i = 0; i < n && g_imgCount < 512; i++) {
            Il2CppImage* im = IL2CPP::assembly_get_image(as[i]);
            if (im) g_imgs[g_imgCount++] = im;
        }
        Log("assemblies=%d imagens=%d", (int)n, g_imgCount);
    }

    static Il2CppClass* Cls(const char* name, const char* ns = "") {
        for (int i = 0; i < g_imgCount; i++) {
            Il2CppClass* c = IL2CPP::class_from_name(g_imgs[i], ns, name);
            if (c) return c;
        }
        return nullptr;
    }
    Il2CppClass* FindClass(const char* name, const char* ns) { return Cls(name, ns ? ns : ""); }

    static const MethodInfo* M(Il2CppClass* c, const char* name, int argc) {
        return c ? IL2CPP::class_get_method_from_name(c, name, argc) : nullptr;
    }
    static const MethodInfo* MAny(Il2CppClass* c, std::initializer_list<const char*> names, int argc) {
        for (auto n : names) {
            const MethodInfo* m = M(c, n, argc);
            if (m) return m;
        }
        return nullptr;
    }
    static FieldInfo* FieldOf(Il2CppClass* c, std::initializer_list<const char*> names) {
        if (!c) return nullptr;
        for (auto n : names) {
            FieldInfo* f = IL2CPP::class_get_field_from_name(c, n);
            if (f) return f;
        }
        return nullptr;
    }
    static size_t Off(Il2CppClass* c, const char* what, std::initializer_list<const char*> names) {
        FieldInfo* f = FieldOf(c, names);
        size_t o = f ? IL2CPP::field_get_offset(f) : 0;
        Log("  campo %-14s = 0x%X %s", what, (unsigned)o, f ? "" : "(NAO ACHADO)");
        return o;
    }
    static bool IsBlacklisted(Il2CppClass* k) {
        const char* n = SafeClassName(k);
        if (!n) return true;
        static const char* bad[] = { "Vehicle", "Bullet", "Projectile", "Pickup", "Grenade",
                                     "Decal", "Tracer", "Camera", "Audio", "UI", "Manager" };
        for (auto b : bad) if (strstr(n, b)) return true;
        return false;
    }

    const char* SafeClassName(Il2CppClass* k) {
        if (!k || !IL2CPP::class_get_name) return "?";
        __try { return IL2CPP::class_get_name(k); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
    }
    void* SafeReadObjPub(void* o, size_t off) {
        if (!o || !off) return nullptr;
        void* v = nullptr;
        __try { v = *(void**)((uintptr_t)o + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return v;
    }

    // --------------------------------------------------------------------- init
    bool Init() {
        InitializeCriticalSection(&g_targetCs);
        CollectImages();
        if (!g_imgCount) { Log("sem assemblies"); return false; }

        kEntity = Cls("Entity");
        kNPC = Cls("NPC");
        kPlayerScript = Cls("PlayerScript");
        kPlayer = kPlayerScript;                 // alias legado
        kTransform = Cls("Transform", "UnityEngine");
        kComponent = Cls("Component", "UnityEngine");
        kCamera = Cls("Camera", "UnityEngine");
        kScreen = Cls("Screen", "UnityEngine");
        kCursor = Cls("Cursor", "UnityEngine");
        kPhysics = Cls("Physics", "UnityEngine");
        kAntiHack = Cls("PlayerAntiHack");
        Il2CppClass* kGM = Cls("GameManager");
        Il2CppClass* kObj = Cls("Object", "UnityEngine");
        Il2CppClass* kGun = Cls("FirstPersonGunView");
        Il2CppClass* kStats = Cls("GunStats");

        Log("classes: entity=%p npc=%p player=%p cam=%p gun=%p stats=%p gm=%p antihack=%p",
            (void*)kEntity, (void*)kNPC, (void*)kPlayerScript, (void*)kCamera,
            (void*)kGun, (void*)kStats, (void*)kGM, (void*)kAntiHack);

        Il2CppClass* kScan = kEntity ? kEntity : kPlayerScript;
        if (!kScan) { Log("nem Entity nem PlayerScript encontrados"); return false; }
        Log("classe de scan = %s", SafeClassName(kScan));

        Il2CppClass* kP = kPlayerScript ? kPlayerScript : kScan;

        Log("[1] campos");
        // nomes confirmados pelo dump
        O_health = Off(kP, "health", { "_health", "health", "hp", "currentHealth", "m_health" });
        O_team = Off(kP, "team", { "mpTeamNumber", "team", "Team", "teamId", "_team" });
        O_rotation = Off(kP, "rotation", { "rotation", "Rotation", "viewRotation" });
        O_camTf = Off(kP, "camTf", { "cameraTransform", "camTransform", "camTf" });
        O_gunStats = Off(kP, "gunStats", { "mpCurrentGunStats", "gunStats", "stats" });
        O_name = Off(kP, "name", { "username", "playerName", "name" });
        O_alive = Off(kP, "alive", { "shouldBeAlive" });
        if (!O_alive) O_alive = Off(kP, "alive", { "isAlive", "alive" });
        O_speed = Off(kP, "speed", { "speed", "moveSpeed", "walkSpeed" });
        O_vel = Off(kP, "velocity", { "lastMoveVelocity", "velocity", "vel" });   // @0x634
        O_gunView = Off(kP, "gunView", { "gunView" });
        if (!O_gunView) O_gunView = O_gunStats;     // PlayerScript nao tem gunView

        if (O_health) {
            const char* hn = IL2CPP::FieldTypeName(kP, "_health");
            if (!hn) hn = IL2CPP::FieldTypeName(kP, "health");
            if (hn) {
                O_healthIsFloat = (strstr(hn, "Single") != nullptr);
                Log("  tipo do health = %s (float=%d)", hn, O_healthIsFloat ? 1 : 0);
            }
        }
        if (O_rotation) {
            const char* rn = IL2CPP::FieldTypeName(kP, "rotation");
            if (rn) {
                O_rotationIsQuat = (strstr(rn, "Quaternion") != nullptr);
                Log("  tipo do rotation = %s (quat=%d)", rn, O_rotationIsQuat ? 1 : 0);
            }
        }

        Log("[2] campos por tipo");
        {
            void* it = nullptr;
            while (FieldInfo* f = IL2CPP::class_get_fields(kP, &it)) {
                const Il2CppType* ft = IL2CPP::field_get_type(f);
                const char* tn = ft ? IL2CPP::type_get_name(ft) : nullptr;
                const char* fn = IL2CPP::field_get_name(f);
                if (!tn || !fn) continue;
                if (!O_camField && strstr(tn, "UnityEngine.Camera")) O_camField = IL2CPP::field_get_offset(f);
                if (!O_gunStats && strstr(tn, "gunStats"))            O_gunStats = IL2CPP::field_get_offset(f);
                if (!kWeapon && (strstr(fn, "GunStats") || strstr(tn, "gunStats")))
                    kWeapon = SafeClassFromType(ft);
            }
            Log("  camField=%d gunStats=0x%X alive=0x%X weaponCls=%s",
                O_camField ? 1 : 0, (unsigned)O_gunStats, (unsigned)O_alive, SafeClassName(kWeapon));
        }

        Log("[3] gun stats");
        {
            Il2CppClass* statsCls = nullptr;
            FieldInfo* gsf = kP ? IL2CPP::class_get_field_from_name(kP, "mpCurrentGunStats") : nullptr;
            if (!gsf && kP) gsf = IL2CPP::class_get_field_from_name(kP, "gunStats");
            if (!gsf && kP) gsf = IL2CPP::class_get_field_from_name(kP, "stats");
            if (gsf) {
                const Il2CppType* st = IL2CPP::field_get_type(gsf);
                statsCls = st ? SafeClassFromType(st) : nullptr;
            }
            if (!statsCls) statsCls = kWeapon;
            if (statsCls) {
                Log("  statsCls=%s", SafeClassName(statsCls));
                IL2CPP::DumpFields(statsCls, "gunstats");
                O_curAmmo = Off(statsCls, "curAmmo", { "currentAmmo", "ammo", "ammoInClip", "clip", "bullets", "currentBullets" });
                O_maxAmmo = Off(statsCls, "maxAmmo", { "maxAmmo", "magSize", "clipSize", "maxBullets", "magazineSize" });
                O_reloading = Off(statsCls, "reloading", { "isReloading", "reloading", "_isReloading" });
                O_spread = Off(statsCls, "spread", { "spread", "spreadAmount", "baseSpread", "currentSpread" });
                O_fireRate = Off(statsCls, "fireRate", { "fireRate", "fireDelay", "rateOfFire", "shotDelay" });
                O_range = Off(statsCls, "range", { "range", "maxRange", "bulletRange" });
            }
            else {
                Log("  statsCls NAO resolvido");
            }
            if (kGun) O_gunStatsOnView = IL2CPP::OffsetOf(kGun, "gunStats");
            Log("  O_gunStatsOnView=0x%X", (unsigned)O_gunStatsOnView);
        }

        Log("[4] gamemanager");
        M_get_instance = MAny(kGM, { "get_instance", "get_Instance" }, 0);
        M_getPlayerScript = MAny(kGM, { "get_playerScript", "get_localPlayer", "get_LocalPlayer" }, 0);
        F_gmInstance = kGM ? FieldOf(kGM, { "instance", "Instance", "singleton", "_instance" }) : nullptr;
        Log("  gm=%s inst=%s player=%s", F_gmInstance ? "OK" : "-",
            M_get_instance ? "OK" : "-", M_getPlayerScript ? "OK" : "-");

        Log("[5] transform");
        M_get_transform = M(kComponent, "get_transform", 0);
        if (!M_get_transform) M_get_transform = M(kTransform, "get_transform", 0);
        M_get_position = M(kTransform, "get_position", 0);
        M_set_position = M(kTransform, "set_position", 1);
        M_get_childCount = M(kTransform, "get_childCount", 0);
        M_GetChild = M(kTransform, "GetChild", 1);
        M_get_rotation = M(kTransform, "get_rotation", 0);
        M_set_rotation = M(kTransform, "set_rotation", 1);
        M_get_forward = M(kTransform, "get_forward", 0);
        M_set_forward = M(kTransform, "set_forward", 1);
        M_LookAt = M(kTransform, "LookAt", 1);
        Log("  transform get=%s pos=%s set=%s child=%s", M_get_transform ? "OK" : "-",
            M_get_position ? "OK" : "-", M_set_position ? "OK" : "-", M_GetChild ? "OK" : "-");

        Log("[6] camera / object");
        M_get_name = kObj ? M(kObj, "get_name", 0) : nullptr;
        M_GetComponentType = kComponent ? M(kComponent, "GetComponent", 1) : nullptr;
        M_get_main = kCamera ? M(kCamera, "get_main", 0) : nullptr;
        M_get_current = kCamera ? M(kCamera, "get_current", 0) : nullptr;
        M_get_allCameras = kCamera ? M(kCamera, "get_allCameras", 0) : nullptr;
        M_WorldToScreen = kCamera ? M(kCamera, "WorldToScreenPoint", 1) : nullptr;
        M_get_proj = kCamera ? M(kCamera, "get_projectionMatrix", 0) : nullptr;
        M_get_view = kCamera ? M(kCamera, "get_worldToCameraMatrix", 0) : nullptr;
        M_get_pw = kCamera ? M(kCamera, "get_pixelWidth", 0) : nullptr;
        M_get_ph = kCamera ? M(kCamera, "get_pixelHeight", 0) : nullptr;
        M_get_fov = kCamera ? M(kCamera, "get_fieldOfView", 0) : nullptr;
        M_set_fov = kCamera ? M(kCamera, "set_fieldOfView", 1) : nullptr;
        M_get_width = kScreen ? M(kScreen, "get_width", 0) : nullptr;
        M_get_height = kScreen ? M(kScreen, "get_height", 0) : nullptr;
        M_set_lockState = kCursor ? M(kCursor, "set_lockState", 1) : nullptr;
        M_set_visible = kCursor ? M(kCursor, "set_visible", 1) : nullptr;
        Log("  w2s=%s fov=%s cursor=%s", M_WorldToScreen ? "OK" : "-",
            M_get_fov ? "OK" : "-", M_set_lockState ? "OK" : "-");

        Log("[7] player / combate");
        M_FireBullet = M(kP, "FireBullet", 7);
        M_shoot = M(kP, "SemiAutoShoot", 0);
        if (!M_shoot) M_shoot = M(kP, "mpPlayShootSound", 0);
        if (!M_shoot) M_shoot = M(kP, "FireBullet", 7);
        M_ComputeRecoil = kGun ? M(kGun, "ComputeShotRecoil", 4) : nullptr;   // argc = 4
        if (!M_ComputeRecoil && kGun) M_ComputeRecoil = M(kGun, "ComputeShotRecoil", 0);
        M_hurt = M(kP, "hurt", 7);
        M_localHurt = M(kP, "localHurt", 5);
        if (!M_localHurt) M_localHurt = M(kP, "damagePlayer", 5);
        M_get_team = MAny(kP, { "get_mpTeamNumber", "get_teamNumber", "get_team" }, 0);
        M_LateUpdate = M(kP, "LateUpdate", 0);
        M_getDisplayName = M(kScan, "GetRawDisplayName", 0);
        M_FindObjects = kObj ? M(kObj, "FindObjectsOfType", 1) : nullptr;
        M_FindObjectsAll = kObj ? M(kObj, "FindObjectsOfTypeAll", 1) : nullptr;
        M_Raycast3 = kPhysics ? M(kPhysics, "Raycast", 3) : nullptr;
        M_Raycast4 = PickRaycast4(kPhysics);
        M_Linecast = kPhysics ? M(kPhysics, "Linecast", 2) : nullptr;
        if (Il2CppClass* kHit = Cls("RaycastHit", "UnityEngine"))
            M_hitDist = M(kHit, "get_distance", 0);

        M_getHealth = MAny(kP, { "get_mpHealth", "get_health", "get_Health" }, 0);
        M_getMaxHealth = MAny(kP, { "get_mpMaxHealth", "get_maxHealth", "get_MaxHealth" }, 0);

        Log("  fire=%s shoot=%s recoil=%s hurt=%s localHurt=%s late=%s find=%s hp=%s team=%s",
            M_FireBullet ? "OK" : "-", M_shoot ? "OK" : "-", M_ComputeRecoil ? "OK" : "-",
            M_hurt ? "OK" : "-", M_localHurt ? "OK" : "-", M_LateUpdate ? "OK" : "-",
            M_FindObjects ? "OK" : "-", M_getHealth ? "OK" : "-", M_get_team ? "OK" : "-");

        if (M_get_width) {
            Il2CppObject* rw = SafeInvoke(M_get_width, nullptr, nullptr);
            int* vw = rw ? (int*)SafeUnbox(rw) : nullptr;
            if (vw) g_sw = (float)*vw;
        }
        if (M_get_height) {
            Il2CppObject* rh = SafeInvoke(M_get_height, nullptr, nullptr);
            int* vh = rh ? (int*)SafeUnbox(rh) : nullptr;
            if (vh) g_sh = (float)*vh;
        }
        Log("  tela = %.0fx%.0f", g_sw, g_sh);

        // =====================================================================
        // AUTO-DUMP
        // =====================================================================
        Log("==================== DUMP DE VERIFICACAO ====================");
        Log("classe de scan = %s   (player = %s)", SafeClassName(kScan), SafeClassName(kP));

        Log("--- campos de %s ---", SafeClassName(kScan));
        IL2CPP::DumpFields(kScan, "scan");
        if (kP != kScan) {
            Log("--- campos de %s ---", SafeClassName(kP));
            IL2CPP::DumpFields(kP, "player");
        }

        Log("--- metodos de dano em %s ---", SafeClassName(kScan));
        IL2CPP::DumpMethodsLike(kScan, "Damage");
        IL2CPP::DumpMethodsLike(kScan, "Hurt");
        IL2CPP::DumpMethodsLike(kScan, "Kill");

        Log("--- metodos de dano em %s ---", SafeClassName(kP));
        IL2CPP::DumpMethodsLike(kP, "hurt");
        IL2CPP::DumpMethodsLike(kP, "Hurt");
        IL2CPP::DumpMethodsLike(kP, "localHurt");
        IL2CPP::DumpMethodsLike(kP, "Damage");

        Log("--- metodos de tiro em %s ---", SafeClassName(kP));
        IL2CPP::DumpMethodsLike(kP, "Fire");
        IL2CPP::DumpMethodsLike(kP, "Shoot");
        IL2CPP::DumpMethodsLike(kP, "shot");
        if (kGun) {
            Log("--- metodos de tiro em %s ---", SafeClassName(kGun));
            IL2CPP::DumpMethodsLike(kGun, "Shoot");
            IL2CPP::DumpMethodsLike(kGun, "Fire");
            IL2CPP::DumpMethodsLike(kGun, "Recoil");
        }

        Log("--- probe de campos em %s ---", SafeClassName(kP));
        {
            const char* subs[] = { "peed", "eloc", "ove", "jump", "Jump", "Fly", "Grounded",
                                   "Walk", "Run", "Sprint", "Crouch", "Gun", "eapon" };
            void* it = nullptr;
            int shown = 0;
            while (FieldInfo* f = IL2CPP::class_get_fields(kP, &it)) {
                const char* fn = IL2CPP::field_get_name(f);
                if (!fn) continue;
                for (int s = 0; s < 13; s++) {
                    if (strstr(fn, subs[s])) {
                        const Il2CppType* ft = IL2CPP::field_get_type(f);
                        const char* tn = (ft && IL2CPP::type_get_name) ? IL2CPP::type_get_name(ft) : "?";
                        Log("    %-28s : %-34s @0x%X", fn, tn ? tn : "?", (unsigned)IL2CPP::field_get_offset(f));
                        shown++;
                        break;
                    }
                }
                if (shown > 70) break;
            }
            Log("  (probe: %d campos)", shown);
        }

        Log("--- campos criticos em %s ---", SafeClassName(kP));
        const char* probes[] = { "health", "_health", "mpHealth", "mpTeamNumber", "username",
                                 "shouldBeAlive", "forcedAlive", "isAlive", "rotation",
                                 "mpCurrentGunStats", "cameraTransform", "isMine", "entityId" };
        for (int i = 0; i < 13; i++) {
            const char* tn = IL2CPP::FieldTypeName(kP, probes[i]);
            if (tn) Log("    %-20s : %-40s @0x%X", probes[i], tn, (unsigned)IL2CPP::OffsetOf(kP, probes[i]));
        }

        Log("==================== FIM DO DUMP ====================");
        Log("sdk init done");
        return true;
    }

    // ------------------------------------------------------------------ getters
    bool IsA(Il2CppObject* o, Il2CppClass* c) {
        if (!o || !c || !o->klass) return false;
        if (o->klass == c) return true;
        if (IL2CPP::class_get_parent) {
            Il2CppClass* k = o->klass;
            for (int i = 0; k && i < 32; i++) {
                Il2CppClass* par = IL2CPP::class_get_parent(k);
                if (!par || par == k) break;
                if (par == c) return true;
                k = par;
            }
        }
        const char* cn = SafeClassName(o->klass);
        const char* tn = SafeClassName(c);
        return cn && tn && strcmp(cn, tn) == 0;
    }

    Il2CppObject* GameManager() {
        if (F_gmInstance) { if (Il2CppObject* gm = SafeStaticGet(F_gmInstance)) return gm; }
        if (M_get_instance) return SafeInvoke(M_get_instance, nullptr, nullptr);
        return nullptr;
    }
    Il2CppObject* LocalPlayer() {
        Il2CppObject* gm = GameManager();
        if (!gm || !M_getPlayerScript) return nullptr;
        return SafeInvoke(M_getPlayerScript, (void*)gm, nullptr);
    }
    Il2CppObject* MainCamera() { return M_get_main ? SafeInvoke(M_get_main, nullptr, nullptr) : nullptr; }
    Il2CppObject* GetTransform(Il2CppObject* o) { return (o && M_get_transform) ? SafeInvoke(M_get_transform, (void*)o, nullptr) : nullptr; }

    Il2CppObject* GetComponentType(Il2CppObject* o, Il2CppClass* klass) {
        if (!o || !klass || !M_GetComponentType || !IL2CPP::class_get_type || !IL2CPP::type_get_object) return nullptr;
        const Il2CppType* tp = IL2CPP::class_get_type(klass);
        if (!tp) return nullptr;
        Il2CppObject* to = (Il2CppObject*)IL2CPP::type_get_object(tp);
        if (!to) return nullptr;
        void* a[1] = { (void*)to };
        return SafeInvoke(M_GetComponentType, (void*)o, a);
    }

    Vec3 GetPos(Il2CppObject* t) {
        if (!t || !M_get_position) return Vec3();
        Il2CppObject* r = SafeInvoke(M_get_position, (void*)t, nullptr);
        Vec3* v = r ? (Vec3*)SafeUnbox(r) : nullptr;
        return v ? *v : Vec3();
    }
    Vec3 GetForward(Il2CppObject* t) {
        if (!t || !M_get_forward) return Vec3(0, 0, 1);
        Il2CppObject* r = SafeInvoke(M_get_forward, (void*)t, nullptr);
        Vec3* v = r ? (Vec3*)SafeUnbox(r) : nullptr;
        return v ? *v : Vec3(0, 0, 1);
    }
    Quat GetRot(Il2CppObject* t) {
        Quat q{ 0, 0, 0, 1 };
        if (!t || !M_get_rotation) return q;
        Il2CppObject* r = SafeInvoke(M_get_rotation, (void*)t, nullptr);
        Quat* v = r ? (Quat*)SafeUnbox(r) : nullptr;
        return v ? *v : q;
    }
    bool SetRot(Il2CppObject* t, const Quat& q) {
        if (!t || !M_set_rotation) return false;
        Quat v = q;
        void* a[1] = { &v };
        return SafeInvoke(M_set_rotation, (void*)t, a) != nullptr;
    }

    float InvokeFloat(const MethodInfo* m, Il2CppObject* o, float fb) {
        if (!m || !o) return fb;
        Il2CppObject* r = SafeInvoke(m, (void*)o, nullptr);
        void* v = r ? SafeUnbox(r) : nullptr;
        if (!v) return fb;
        __try {
            int32_t asInt = *(int32_t*)v;
            float   asFlt = *(float*)v;
            bool intOk = (asInt >= 0 && asInt <= 100000);
            bool fltOk = (asFlt >= 0.f && asFlt <= 100000.f);
            if (intOk && !fltOk) return (float)asInt;
            if (fltOk && !intOk) return asFlt;
            if (intOk && fltOk)  return (asInt <= 1000) ? (float)asInt : asFlt;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return fb;
    }

    float GetHealthPct(Il2CppObject* p) {
        if (!p) return 100.f;

        float hp = -1.f, mx = 100.f;
        if (M_getHealth)    hp = InvokeFloat(M_getHealth, p, -1.f);
        if (M_getMaxHealth) mx = InvokeFloat(M_getMaxHealth, p, 100.f);
        if (mx <= 1.f || mx > 100000.f) mx = 100.f;

        if (hp < 0.f && O_health) {
            if (O_healthIsFloat) hp = SafeReadFloat(p, O_health, -1.f);
            else {
                int v = 0;
                if (O_healthValue) {
                    void* raw = SafeReadObjPub(p, O_health);
                    if (raw) v = SafeReadInt(raw, O_healthValue, 0);
                }
                if (v <= 0 || v > 100000) v = SafeReadInt(p, O_health, -1);
                hp = (float)v;
            }
        }
        if (hp < 0.f || hp > 100000.f) hp = 100.f;

        float pct = hp * 100.f / mx;
        if (pct < 0.f) pct = 0.f;
        if (pct > 100.f) pct = 100.f;
        return pct;
    }

    int GetHealth(Il2CppObject* p) { return (int)(GetHealthPct(p) + 0.5f); }

    bool ReadAliveField(Il2CppObject* p, bool fb) {
        if (!p || !O_alive) return fb;
        return SafeReadBool(p, O_alive, fb);
    }
    bool IsAlive(Il2CppObject* p) {
        if (!p) return false;
        if (!ReadAliveField(p, true)) return false;
        return GetHealthPct(p) > 0.5f;
    }

    int GetTeam(Il2CppObject* p) {
        if (!p) return -1;
        if (M_get_team) {
            Il2CppObject* r = SafeInvoke(M_get_team, (void*)p, nullptr);
            int* v = r ? (int*)SafeUnbox(r) : nullptr;
            if (v) return *v;
        }
        return O_team ? SafeReadInt(p, O_team, -1) : -1;
    }

    // SEH-safe: sem std::string local
    void GetNameBuf(Il2CppObject* p, char* out, int outSize) {
        if (outSize < 2) return;
        out[0] = 0;
        if (!p) return;

        if (M_getDisplayName) {
            if (Il2CppObject* r = SafeInvoke(M_getDisplayName, (void*)p, nullptr)) {
                IL2CPP::StrToUtf8(r, out, outSize);
                if (out[0]) return;
            }
        }
        if (M_get_name) {
            if (Il2CppObject* r = SafeInvoke(M_get_name, (void*)p, nullptr)) {
                IL2CPP::StrToUtf8(r, out, outSize);
                if (out[0]) return;
            }
        }
        if (O_name) {
            if (void* s = SafeReadObjPub(p, O_name))
                IL2CPP::StrToUtf8((Il2CppObject*)s, out, outSize);
        }
    }

    std::string GetName(Il2CppObject* p) {
        char buf[256];
        GetNameBuf(p, buf, sizeof(buf));
        return buf[0] ? std::string(buf) : std::string("?");
    }

    Il2CppObject* FindBoneExato(Il2CppObject* t, const char* nome, int depth) {
        if (!t || depth > 8 || !M_GetChild || !M_get_childCount || !M_get_name) return nullptr;

        Il2CppObject* nr = SafeInvoke(M_get_name, (void*)t, nullptr);
        if (nr) {
            std::string n = IL2CPP::ToStr((Il2CppString*)nr);
            if (n == nome) return t;
        }

        Il2CppObject* cco = SafeInvoke(M_get_childCount, (void*)t, nullptr);
        int* ccv = cco ? (int*)SafeUnbox(cco) : nullptr;
        int cc = ccv ? *ccv : 0;
        if (cc > 200) cc = 200;
        for (int i = 0; i < cc; i++) {
            void* a[1] = { (void*)&i };
            Il2CppObject* c = SafeInvoke(M_GetChild, (void*)t, a);
            if (Il2CppObject* r = FindBoneExato(c, nome, depth + 1)) return r;
        }
        return nullptr;
    }

    Il2CppObject* CamTfRaw() {
        Il2CppObject* lp = g_localPlayer;
        if (!lp || !O_camTf) return nullptr;
        return (Il2CppObject*)SafeReadObjPub(lp, O_camTf);
    }

    Il2CppObject* CameraObj() {
        Il2CppObject* lp = g_localPlayer;
        if (lp) {
            if (O_camField) { if (Il2CppObject* c = (Il2CppObject*)SafeReadObjPub(lp, O_camField)) return c; }
            if (Il2CppObject* tf = CamTfRaw())
                if (kCamera) if (Il2CppObject* c = GetComponentType(tf, kCamera)) return c;
        }
        if (Il2CppObject* c = MainCamera()) return c;
        if (M_get_allCameras) {
            Il2CppObject* arr = SafeInvoke(M_get_allCameras, nullptr, nullptr);
            if (arr) {
                int cnt = (int)ArrayLen(arr);
                Il2CppObject** d = (Il2CppObject**)ArrayData(arr);
                Il2CppObject* best = nullptr; float bd = 1e9f;
                for (int i = 0; i < cnt; i++) {
                    Il2CppObject* cc = d[i];
                    if (!cc) continue;
                    Il2CppObject* t = GetTransform(cc);
                    if (!t) continue;
                    float dd = GetPos(t).dist(g_localPos);
                    if (dd < bd) { bd = dd; best = cc; }
                }
                if (best) return best;
            }
        }
        return M_get_current ? SafeInvoke(M_get_current, nullptr, nullptr) : nullptr;
    }

    Vec3 LocalPos() {
        Il2CppObject* lp = g_localPlayer;
        if (!lp) return Vec3();
        return GetPos(GetTransform(lp));
    }

    bool WorldToScreen(const Vec3& w, Vec2& out) {
        if (!g_camera || !M_WorldToScreen) return false;
        Vec3 p = w;
        void* a[1] = { (void*)&p };
        Il2CppObject* r = SafeInvoke(M_WorldToScreen, g_camera, a);
        Vec3* v = r ? (Vec3*)SafeUnbox(r) : nullptr;
        if (!v) return false;
        out.x = v->x; out.y = v->y;
        return v->z > 0.01f;
    }

    bool VisibleLine(const Vec3& from, const Vec3& to) {
        if (!M_Linecast) return true;
        Vec3 dir = to - from;
        float d = dir.len();
        if (d < 4.0f) return true;
        Vec3 nrm(dir.x / d, dir.y / d, dir.z / d);
        const float offStart = 1.2f, offEnd = 1.5f;
        if (offStart + offEnd >= d - 0.5f) return true;
        Vec3 start(from.x + nrm.x * offStart, from.y + nrm.y * offStart, from.z + nrm.z * offStart);
        Vec3 end(to.x - nrm.x * offEnd, to.y - nrm.y * offEnd, to.z - nrm.z * offEnd);
        void* a[2] = { (void*)&start, (void*)&end };
        Il2CppObject* r = SafeInvoke(M_Linecast, nullptr, a);
        bool* any = r ? (bool*)SafeUnbox(r) : nullptr;
        return any ? !(*any) : true;
    }

    Il2CppObject* LocalGun() {
        Il2CppObject* p = g_localPlayer;
        return (p && O_gunView) ? (Il2CppObject*)SafeReadObjPub(p, O_gunView) : nullptr;
    }
} // namespace SDK

// ============================================================================
// AddTarget — MAIN THREAD
// ============================================================================
static void FillName(std::string& dst, Il2CppObject* p) {
    char buf[256];
    SDK::GetNameBuf(p, buf, sizeof(buf));
    if (buf[0] && strcmp(buf, "?")) dst = buf;
}

void AddTarget(Il2CppObject* p, bool trust) {
    if (!p || !p->klass) return;

    Il2CppClass* kRef = SDK::kEntity ? SDK::kEntity : SDK::kPlayerScript;
    if (!trust && kRef && !SDK::IsA(p, kRef)) { SDK::dbg_reject++; return; }
    if (SDK::IsBlacklisted(p->klass)) { SDK::dbg_reject++; return; }

    volatile int step = 0;
    EnterCriticalSection(&g_targetCs);
    __try {
        step = 1;
        int slot = -1;
        for (int i = 0; i < g_targetCount; i++) if (g_targets[i].player == p) { slot = i; break; }
        if (slot < 0 && g_targetCount < 64) slot = g_targetCount++;

        if (slot >= 0) {
            Target& t = g_targets[slot];

            step = 2;
            if (t.klass != (void*)p->klass || t.headBone == nullptr || t.footBone == nullptr) {
                t.klass = (void*)p->klass;
                t.headBone = nullptr; t.footBone = nullptr;
                Il2CppObject* tf = SDK::GetTransform(p);
                if (tf) {
                    t.headBone = SDK::FindBoneExato(tf, "Head", 0);
                    if (!t.headBone) t.headBone = SDK::FindBoneExato(tf, "head", 0);
                    if (!t.headBone) t.headBone = SDK::FindBoneExato(tf, "neck", 0);
                    t.footBone = SDK::FindBoneExato(tf, "Foot", 0);
                    if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "foot", 0);
                    if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "Ankle", 0);
                    if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "Toe", 0);
                }
            }
            t.player = p;

            step = 3;
            t.hpMax = 100.f;
            t.hp = SDK::GetHealthPct(p);
            t.health = (int)(t.hp + 0.5f);
            t.alive = (t.hp > 0.5f);

            step = 4;
            Il2CppObject* tf = SDK::GetTransform(p);
            if (tf) {
                step = 5;
                Vec3 base = SDK::GetPos(tf);
                float H = C.espBodyH; if (H < 0.5f || H > 3.f) H = 1.70f;

                Vec3 head, feet;
                if (t.headBone && t.footBone) { head = SDK::GetPos(t.headBone); feet = SDK::GetPos(t.footBone); }
                else if (t.headBone) { head = SDK::GetPos(t.headBone); feet = head - Vec3(0.f, H, 0.f); }
                else if (t.footBone) { feet = SDK::GetPos(t.footBone); head = feet + Vec3(0.f, H, 0.f); }
                else { head = base; feet = base - Vec3(0.f, H, 0.f); }

                float wh = head.dist(feet);
                if (wh < 0.6f || wh > 3.2f) feet = head - Vec3(0.f, H, 0.f);

                t.head = head; t.feet = feet;

                // ---- projeta AQUI (main thread) e cacheia pro render thread ----
                {
                    Vec2 sp, sf;
                    if (SDK::WorldToScreen(head, sp) && SDK::WorldToScreen(feet, sf)) {
                        t.scrHead = sp;
                        t.scrFeet = sf;
                        t.onScreen = true;
                    }
                    else {
                        t.onScreen = false;
                    }
                }

                step = 6;
                t.team = SDK::GetTeam(p);
                step = 7;
                t.dist = g_localPos.dist(t.feet);
                step = 8;
                if (t.name.empty() || t.name == "?") FillName(t.name, p);

                if (C.espWeapon && t.weapon.empty() && SDK::O_gunView) {
                    if (Il2CppObject* gun = (Il2CppObject*)SDK::SafeReadObjPub(p, SDK::O_gunView)) {
                        char wbuf[128];
                        SDK::GetNameBuf(gun, wbuf, sizeof(wbuf));
                        if (wbuf[0] && strcmp(wbuf, "?")) t.weapon = wbuf;
                    }
                }
            }
            step = 9;
            t.seen = GetTickCount64();
            SDK::dbg_adds++;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static ULONGLONG l = 0;
        if (GetTickCount64() - l > 2000) {
            l = GetTickCount64();
            Log("!!! EXCECAO em AddTarget passo %d (p=%p)", (int)step, (void*)p);
        }
    }
    LeaveCriticalSection(&g_targetCs);
}

void PruneTargets() {
    uint64_t now = GetTickCount64();
    EnterCriticalSection(&g_targetCs);
    int w = 0;
    for (int i = 0; i < g_targetCount; i++) {
        if (now - g_targets[i].seen > 500) continue;
        if (w != i) g_targets[w] = g_targets[i];
        w++;
    }
    g_targetCount = w;
    LeaveCriticalSection(&g_targetCs);
}

// ============================================================================
// ScanAllTargets
// ============================================================================
void ScanAllTargets() {
    if (!SDK::M_FindObjects || !IL2CPP::class_get_type || !IL2CPP::type_get_object) return;

    auto scanClass = [](Il2CppClass* k) -> int {
        if (!k) return 0;
        const Il2CppType* tp = IL2CPP::class_get_type(k);
        if (!tp) return 0;
        Il2CppObject* to = (Il2CppObject*)IL2CPP::type_get_object(tp);
        if (!to) return 0;
        void* a[1] = { (void*)to };
        Il2CppObject* arr = IL2CPP::Invoke(SDK::M_FindObjects, nullptr, a);
        if (!arr) return 0;
        int n = (int)ArrayLen(arr);
        void** d = ArrayData(arr);
        int added = 0;
        for (int i = 0; i < n && i < 512; i++) {
            Il2CppObject* o = (Il2CppObject*)d[i];
            if (!o || o == g_localPlayer) continue;
            if (!SDK::IsA(o, k)) continue;
            AddTarget(o, true);
            added++;
        }
        return added;
        };

    // Entity cobre PlayerScript e NPC -> sem scan duplicado
    int total = 0;
    if (SDK::kEntity)            total += scanClass(SDK::kEntity);
    else if (SDK::kPlayerScript) total += scanClass(SDK::kPlayerScript);

    if (g_localPlayer) AddTarget(g_localPlayer, true);

    static int lastLogged = -1;
    if (total != lastLogged) {
        lastLogged = total;
        Log("scan: %d objetos (alvos=%d)", total, g_targetCount);
    }
    SDK::dbg_scanCount = total;
}