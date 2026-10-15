#include "sdk.h"
#include "config.h"

Target g_targets[64];
int g_targetCount = 0;
CRITICAL_SECTION g_targetCs;

namespace SDK {
    Il2CppClass* kPlayer, * kTransform, * kComponent, * kCamera, * kScreen, * kCursor, * kPhysics, * kAntiHack, * kWeapon;
    size_t O_health, O_healthValue, O_team, O_rotation, O_camTf, O_camField, O_alive, O_gunView, O_gunStats, O_name;
    size_t O_curAmmo, O_maxAmmo, O_reloading, O_spread, O_fireRate, O_range, O_speed, O_vel;
    void* F_gmInstance = nullptr;
    const MethodInfo* M_getPlayerScript;
    const MethodInfo* M_get_instance;
    const MethodInfo* M_get_transform, * M_get_position, * M_set_position;
    const MethodInfo* M_get_childCount, * M_GetChild, * M_get_name, * M_get_rotation;
    const MethodInfo* M_get_main, * M_get_current, * M_get_allCameras;
    const MethodInfo* M_WorldToScreen, * M_get_proj, * M_get_view, * M_get_pw, * M_get_ph, * M_get_fov;
    const MethodInfo* M_GetComponentType;
    const MethodInfo* M_get_width, * M_get_height;
    const MethodInfo* M_set_lockState, * M_set_visible;
    const MethodInfo* M_FireBullet, * M_ComputeRecoil, * M_hurt, * M_localHurt, * M_get_team, * M_LateUpdate, * M_shoot;
    const MethodInfo* M_get_forward, * M_set_forward, * M_LookAt, * M_Raycast3;
    const MethodInfo* M_Raycast4 = nullptr;
    const MethodInfo* M_Linecast = nullptr;
    const MethodInfo* M_hitDist = nullptr;
    const MethodInfo* M_getHealth, * M_getMaxHealth;
    const MethodInfo* M_FindObjects, * M_FindObjectsAll;
    int dbg_scanCount = 0, dbg_ok_late = 0, dbg_ok_w2s = 0, dbg_ok_find = 0, dbg_ok_look = 0, dbg_ok_camTf = 0, dbg_ok_cam = 0, dbg_drawn = 0, dbg_teamSkip = 0, dbg_reject = 0, dbg_adds = 0;
    Il2CppObject* dbg_local = nullptr;

    static Il2CppImage* g_img = nullptr;
    static Il2CppImage* g_imgs[512];
    static int g_imgCount = 0;
    static float g_sw = 0.f, g_sh = 0.f;
    static bool g_csReady = false;

    float ScreenW() { return g_sw; }
    float ScreenH() { return g_sh; }

    // ===========================================================================
    // helpers SEH-safe
    // ===========================================================================
    static Il2CppObject* SafeInvoke(const MethodInfo* m, void* o, void** args) {
        if (!m) return nullptr;
        __try { return (Il2CppObject*)IL2CPP::Invoke(m, o, args); }
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
    static Il2CppObject* SafeStaticGet(void* field) {
        if (!field) return nullptr;
        void* v = nullptr;
        __try { IL2CPP::field_static_get_value(field, (void*)&v); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return (Il2CppObject*)v;
    }
    static Il2CppClass* SafeClassFromType(const Il2CppType* t) {
        if (!t || !IL2CPP::class_from_type) return nullptr;
        __try { return IL2CPP::class_from_type(t); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static const char* TypeClassName(const Il2CppType* t) {
        if (!t) return nullptr;
        __try {
            Il2CppClass* k = *(Il2CppClass**)((uintptr_t)t);
            if (!k) return nullptr;
            return IL2CPP::class_get_name(k);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }
    static const char* SafeTypeName(const Il2CppType* t) {
        if (!t) return nullptr;
        const char* n = nullptr;
        __try { n = IL2CPP::type_get_name(t); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return n;
    }

    static inline int ArrLen(Il2CppObject* a) {
        if (!a) return 0;
        return (int)*(uintptr_t*)((uintptr_t)a + 0x18);
    }
    static inline Il2CppObject* ArrAt(Il2CppObject* a, int i) {
        if (!a || i < 0) return nullptr;
        void** d = (void**)((uintptr_t)a + 0x20);
        return (Il2CppObject*)d[i];
    }

    static const Il2CppType* Param0(const MethodInfo* m) {
        if (!m) return nullptr;
        void** p = nullptr;
        __try { p = *(void***)((uintptr_t)m + 0x28); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return p ? (const Il2CppType*)p[0] : nullptr;
    }

    static float HitDistOf(const unsigned char* hit) {
        float d = 0.f;
        __try { d = *(const float*)(hit + 0x1C); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.f; }
        return d;
    }

    static const MethodInfo* PickRaycast4(Il2CppClass* k) {
        if (!k) return nullptr;
        const MethodInfo* weak = nullptr;
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

            if (!weak) weak = m;
            return m;
        }
        return weak;
    }

    // ===========================================================================
    // resolucao de classes -> varre TODAS as assemblies
    // ===========================================================================
    static void CollectImages() {
        size_t n = 0;
        Il2CppAssembly** as = IL2CPP::domain_get_assemblies(IL2CPP::domain_get(), &n);
        if (!as) return;
        if (n > 512) n = 512;
        for (size_t i = 0; i < n && g_imgCount < 512; i++) {
            Il2CppImage* im = IL2CPP::assembly_get_image(as[i]);
            if (im) g_imgs[g_imgCount++] = im;
        }
        if (g_imgCount > 0) g_img = g_imgs[0];
        Log("assemblies = %d | imagens = %d | principal = %p", (int)n, g_imgCount, (void*)g_img);
    }

    static Il2CppClass* Cls(const char* name, const char* ns = "") {
        if (g_img) {
            Il2CppClass* c = IL2CPP::class_from_name(g_img, ns, name);
            if (c) return c;
        }
        for (int i = 0; i < g_imgCount; i++) {
            Il2CppClass* c = IL2CPP::class_from_name(g_imgs[i], ns, name);
            if (c) return c;
        }
        return nullptr;
    }

    static const MethodInfo* M(Il2CppClass* c, const char* name, int argc) {
        if (!c) return nullptr;
        return IL2CPP::class_get_method_from_name(c, name, argc);
    }
    static void* FieldOf(Il2CppClass* c, std::initializer_list<const char*> names) {
        if (!c) return nullptr;
        for (auto n : names) { void* f = IL2CPP::class_get_field_from_name(c, n); if (f) return f; }
        return nullptr;
    }
    static size_t Off(Il2CppClass* c, const char* what, std::initializer_list<const char*> names) {
        void* f = FieldOf(c, names);
        size_t o = f ? IL2CPP::field_get_offset(f) : 0;
        Log("  campo %-14s = 0x%X %s", what, (unsigned)o, f ? "" : "(NAO ACHADO)");
        return o;
    }

    Il2CppClass* FindClass(const char* name, const char* ns) { return Cls(name, ns ? ns : ""); }

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

    // ===========================================================================
    // init
    // ===========================================================================
    bool Init() {
        if (!g_csReady) { InitializeCriticalSection(&g_targetCs); g_csReady = true; }

        CollectImages();
        if (!g_img) { Log("sem assemblies"); return false; }

        kPlayer = Cls("PlayerScript");
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
        Log("classes: player=%p cam=%p screen=%p obj=%p gun=%p stats=%p gm=%p antihack=%p",
            (void*)kPlayer, (void*)kCamera, (void*)kScreen, (void*)kObj, (void*)kGun, (void*)kStats, (void*)kGM, (void*)kAntiHack);
        if (!kPlayer) return false;
        dbg_ok_cam = kCamera ? 1 : 0;

        Log("[1] campos");
        O_health = Off(kPlayer, "health", { "health", "Health", "hp", "_health", "currentHealth", "m_health" });
        O_team = Off(kPlayer, "team", { "team", "Team", "teamId", "TeamId", "_team", "side" });
        O_rotation = Off(kPlayer, "rotation", { "rotation", "Rotation", "viewRotation", "_rotation" });
        O_camTf = Off(kPlayer, "camTf", { "cameraTransform", "camTransform", "camTf", "_cameraTransform" });
        O_gunView = Off(kPlayer, "gunView", { "gunView", "firstPersonGunView", "_gunView", "gun" });
        O_gunStats = Off(kPlayer, "gunStats", { "gunStats", "stats", "_gunStats" });
        O_name = Off(kPlayer, "name", { "username", "playerName", "name", "_name", "nickname" });

        if (O_health) {
            void* hf = IL2CPP::class_get_field_from_name(kPlayer, "_health");
            if (!hf) hf = IL2CPP::class_get_field_from_name(kPlayer, "health");
            if (hf) {
                const Il2CppType* ht = IL2CPP::field_get_type(hf);
                Il2CppClass* hc = ht ? SafeClassFromType(ht) : nullptr;
                if (hc) {
                    Log("  tipo do _health = %s", SafeClassName(hc));
                    void* vf = IL2CPP::class_get_field_from_name(hc, "Value");
                    if (!vf) vf = IL2CPP::class_get_field_from_name(hc, "value");
                    if (vf) {
                        O_healthValue = IL2CPP::field_get_offset(vf);
                        Log("  _health.Value = 0x%X", (unsigned)O_healthValue);
                    }
                }
            }
        }

        Log("[2] campos por tipo");
        {
            void* it = nullptr; void* f = nullptr;
            while ((f = IL2CPP::class_get_fields(kPlayer, &it))) {
                const Il2CppType* ft = IL2CPP::field_get_type(f);
                const char* tn = ft ? IL2CPP::type_get_name(ft) : nullptr;
                const char* fn = IL2CPP::field_get_name(f);
                if (!tn || !fn) continue;
                if (!O_camField && strstr(tn, "UnityEngine.Camera")) O_camField = IL2CPP::field_get_offset(f);
                if (!O_gunStats && (strstr(tn, "gunStats") || strstr(tn, "GunStats"))) O_gunStats = IL2CPP::field_get_offset(f);
                if (!O_alive && strstr(fn, "shouldBeAlive")) O_alive = IL2CPP::field_get_offset(f);
                if (!kWeapon && strstr(fn, "weapon") && !strstr(tn, "System"))
                    kWeapon = SafeClassFromType(ft);
            }
            Log("  por tipo: camField=%d gunStats=0x%X alive=0x%X weaponCls=%p",
                O_camField ? 1 : 0, (unsigned)O_gunStats, (unsigned)O_alive, (void*)kWeapon);
        }

        // =======================================================================
        // [3] gunstats via TIPO REAL do campo (nao via Cls("GunStats") que falha)
        // =======================================================================
        Log("[3] gunstats (via tipo do campo, nao via Cls)");
        {
            Il2CppClass* statsCls = nullptr;
            void* gsf = IL2CPP::class_get_field_from_name(kPlayer, "gunStats");
            if (!gsf) gsf = IL2CPP::class_get_field_from_name(kPlayer, "stats");
            if (gsf) {
                const Il2CppType* st = IL2CPP::field_get_type(gsf);
                statsCls = st ? SafeClassFromType(st) : nullptr;
                Log("  gunStats field=%p tipo=%s", gsf, statsCls ? SafeClassName(statsCls) : "?");
            }
            if (!statsCls && kWeapon) {
                statsCls = kWeapon;
                Log("  fallback: usando weaponCls=%p", (void*)statsCls);
            }
            if (statsCls) {
                Log("  --- campos de %s ---", SafeClassName(statsCls));
                void* it = nullptr; void* f = nullptr;
                int n = 0;
                while ((f = IL2CPP::class_get_fields(statsCls, &it)) && n < 80) {
                    const char* fn = IL2CPP::field_get_name(f);
                    const Il2CppType* ft = IL2CPP::field_get_type(f);
                    const char* tn = (ft && IL2CPP::type_get_name) ? IL2CPP::type_get_name(ft) : "?";
                    Log("    %s : %s @0x%X", fn ? fn : "?", tn ? tn : "?", (unsigned)IL2CPP::field_get_offset(f));
                    n++;
                }
                O_curAmmo = Off(statsCls, "curAmmo", { "currentAmmo", "ammo", "ammoInClip", "clip", "bullets", "currentBullets" });
                O_maxAmmo = Off(statsCls, "maxAmmo", { "maxAmmo", "magSize", "clipSize", "maxBullets", "magazineSize" });
                O_reloading = Off(statsCls, "reloading", { "isReloading", "reloading", "_isReloading", "reload" });
                O_spread = Off(statsCls, "spread", { "spread", "spreadAmount", "baseSpread", "currentSpread" });
                O_fireRate = Off(statsCls, "fireRate", { "fireRate", "fireDelay", "roundsPerMinute", "rateOfFire", "shotDelay" });
                O_range = Off(statsCls, "range", { "range", "maxRange", "bulletRange" });
            }
            else {
                Log("  gunStats tipo NAO resolvido");
            }

            if (kWeapon) {
                Log("  --- campos de weapon (%s) ---", SafeClassName(kWeapon));
                void* it = nullptr; void* f = nullptr;
                int n = 0;
                while ((f = IL2CPP::class_get_fields(kWeapon, &it)) && n < 80) {
                    const char* fn = IL2CPP::field_get_name(f);
                    const Il2CppType* ft = IL2CPP::field_get_type(f);
                    const char* tn = (ft && IL2CPP::type_get_name) ? IL2CPP::type_get_name(ft) : "?";
                    Log("    %s : %s @0x%X", fn ? fn : "?", tn ? tn : "?", (unsigned)IL2CPP::field_get_offset(f));
                    n++;
                }
                if (!O_spread)   O_spread = Off(kWeapon, "spread", { "spread", "spreadAmount", "baseSpread" });
                if (!O_fireRate) O_fireRate = Off(kWeapon, "fireRate", { "fireRate", "fireDelay", "rateOfFire" });
                if (!O_range)    O_range = Off(kWeapon, "range", { "range", "maxRange" });
            }
        }

        Log("[4] gamemanager");
        M_get_instance = M(kGM, "get_instance", 0);
        if (!M_get_instance) M_get_instance = M(kGM, "get_Instance", 0);
        M_getPlayerScript = M(kGM, "get_playerScript", 0);
        if (!M_getPlayerScript) M_getPlayerScript = M(kGM, "get_localPlayer", 0);
        if (!M_getPlayerScript) M_getPlayerScript = M(kGM, "get_LocalPlayer", 0);
        F_gmInstance = kGM ? FieldOf(kGM, { "instance", "Instance", "singleton", "_instance" }) : nullptr;
        Log("  gm.instance=%s getInstance=%s getPlayerScript=%s",
            F_gmInstance ? "OK" : "FALHOU", M_get_instance ? "OK" : "FALHOU", M_getPlayerScript ? "OK" : "FALHOU");

        Log("[5] transform");
        M_get_transform = M(kComponent, "get_transform", 0);
        if (!M_get_transform) M_get_transform = M(kTransform, "get_transform", 0);
        M_get_position = M(kTransform, "get_position", 0);
        M_set_position = M(kTransform, "set_position", 1);
        M_get_childCount = M(kTransform, "get_childCount", 0);
        M_GetChild = M(kTransform, "GetChild", 1);
        M_get_rotation = M(kTransform, "get_rotation", 0);
        M_get_forward = M(kTransform, "get_forward", 0);
        M_set_forward = M(kTransform, "set_forward", 1);
        Log("  transform: get=%s pos=%s child=%s", M_get_transform ? "OK" : "FALHOU",
            M_get_position ? "OK" : "FALHOU", M_GetChild ? "OK" : "FALHOU");

        Log("[6] lookat");
        M_LookAt = nullptr;
        {
            const MethodInfo* la = M(kTransform, "LookAt", 1);
            if (la) {
                const Il2CppType* p0 = Param0(la);
                const char* cn = TypeClassName(p0);
                if (!cn) { Il2CppClass* pc = SafeClassFromType(p0); cn = pc ? SafeClassName(pc) : nullptr; }
                Log("  LookAt(1) param0 tipo = %s", cn ? cn : "?");
                if (cn && strstr(cn, "Vector3")) M_LookAt = la;
            }
            else Log("  LookAt(1) FALHOU");
        }

        Log("[7] camera/objeto");
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
        M_get_width = kScreen ? M(kScreen, "get_width", 0) : nullptr;
        M_get_height = kScreen ? M(kScreen, "get_height", 0) : nullptr;
        M_set_lockState = kCursor ? M(kCursor, "set_lockState", 1) : nullptr;
        M_set_visible = kCursor ? M(kCursor, "set_visible", 1) : nullptr;
        Log("  w2s=%s getName=%s comp=%s", M_WorldToScreen ? "OK" : "FALHOU",
            M_get_name ? "OK" : "FALHOU", M_GetComponentType ? "OK" : "FALHOU");

        Log("[8] player");
        M_FireBullet = M(kPlayer, "FireBullet", 7);
        M_ComputeRecoil = M(kGun, "ComputeShotRecoil", 0);
        M_hurt = M(kPlayer, "hurt", 7);
        M_localHurt = M(kPlayer, "localHurt", 5);
        if (!M_localHurt) M_localHurt = M(kPlayer, "damagePlayer", 5);
        M_get_team = M(kPlayer, "get_teamNumber", 0);
        M_LateUpdate = M(kPlayer, "LateUpdate", 0);
        M_FindObjects = kObj ? M(kObj, "FindObjectsOfType", 1) : nullptr;
        M_Raycast3 = kPhysics ? M(kPhysics, "Raycast", 3) : nullptr;
        M_Raycast4 = PickRaycast4(kPhysics);
        if (!M_Raycast4) M_Raycast4 = M(kPhysics, "Raycast", 4);

        M_Linecast = kPhysics ? M(kPhysics, "Linecast", 2) : nullptr;

        {
            Il2CppClass* kHit = Cls("RaycastHit", "UnityEngine");
            M_hitDist = kHit ? M(kHit, "get_distance", 0) : nullptr;
            Log("  raycast: 3=%s 4=%s linecast=%s dist=%s",
                M_Raycast3 ? "OK" : "FALHOU", M_Raycast4 ? "OK" : "FALHOU",
                M_Linecast ? "OK" : "FALHOU", M_hitDist ? "OK" : "FALHOU");
        }

        M_getHealth = M(kPlayer, "get_mpHealth", 0);
        if (!M_getHealth) M_getHealth = M(kPlayer, "get_health", 0);
        if (!M_getHealth) M_getHealth = M(kPlayer, "get_Health", 0);
        M_getMaxHealth = M(kPlayer, "get_mpMaxHealth", 0);
        if (!M_getMaxHealth) M_getMaxHealth = M(kPlayer, "get_maxHealth", 0);
        if (!M_getMaxHealth) M_getMaxHealth = M(kPlayer, "get_MaxHealth", 0);
        Log("  hurt=%s localHurt=%s team=%s late=%s find=%s health=%s maxHealth=%s",
            M_hurt ? "OK" : "FALHOU", M_localHurt ? "OK" : "FALHOU", M_get_team ? "OK" : "FALHOU",
            M_LateUpdate ? "OK" : "FALHOU", M_FindObjects ? "OK" : "FALHOU",
            M_getHealth ? "OK" : "FALHOU", M_getMaxHealth ? "OK" : "FALHOU");

        dbg_ok_late = M_LateUpdate ? 1 : 0;
        dbg_ok_w2s = M_WorldToScreen ? 1 : 0;
        dbg_ok_find = M_FindObjects ? 1 : 0;
        dbg_ok_camTf = O_camTf ? 1 : 0;
        dbg_ok_look = M_LookAt ? 1 : 0;

        Log("[9] tela");
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

        Log("[10] localplayer");
        Log("  LocalPlayer = %p", (void*)LocalPlayer());

        Log("sdk init done");
        return true;
    }

    // ===========================================================================
    // getters
    // ===========================================================================
    bool IsA(Il2CppObject* o, Il2CppClass* c) {
        if (!o || !c || !o->klass) return false;
        if (o->klass == c) return true;

        if (IL2CPP::class_get_parent) {
            Il2CppClass* k = o->klass;
            for (int i = 0; k && i < 24; i++) {
                Il2CppClass* par = IL2CPP::class_get_parent(k);
                if (!par || par == k) break;
                if (par == c) return true;
                k = par;
            }
        }

        const char* cn = SafeClassName(o->klass);
        const char* tn = SafeClassName(c);
        if (cn && tn && strcmp(cn, tn) == 0) return true;

        return false;
    }

    Il2CppObject* GameManager() {
        if (F_gmInstance) {
            Il2CppObject* gm = SafeStaticGet(F_gmInstance);
            if (gm) return gm;
        }
        if (M_get_instance) {
            Il2CppObject* gm = SafeInvoke(M_get_instance, nullptr, nullptr);
            if (gm) return gm;
        }
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

    float InvokeFloat(const MethodInfo* m, Il2CppObject* o, float fb) {
        if (!m || !o) return fb;
        Il2CppObject* r = SafeInvoke(m, (void*)o, nullptr);
        void* v = r ? SafeUnbox(r) : nullptr;
        if (!v) return fb;

        int32_t asInt = *(int32_t*)v;
        float   asFlt = *(float*)v;
        bool intOk = (asInt >= 0 && asInt <= 100000);
        bool fltOk = (asFlt >= 0.f && asFlt <= 100000.f);

        if (intOk && !fltOk) return (float)asInt;
        if (fltOk && !intOk) return asFlt;
        if (intOk && fltOk)  return (asInt <= 1000) ? (float)asInt : asFlt;
        return fb;
    }

    float GetHealthPct(Il2CppObject* p) {
        if (!p) return 100.f;

        float hp = -1.f, mx = 100.f;
        if (M_getHealth)    hp = InvokeFloat(M_getHealth, p, -1.f);
        if (M_getMaxHealth) mx = InvokeFloat(M_getMaxHealth, p, 100.f);
        if (mx <= 1.f || mx > 100000.f) mx = 100.f;

        if (hp < 0.f) {
            int v = 0;
            if (O_healthValue) {
                void* raw = SafeReadObjPub(p, O_health);
                if (raw) v = SafeReadInt(raw, O_healthValue, 0);
            }
            if (v <= 0 || v > 100000) v = SafeReadInt(p, O_health, 100);
            hp = (float)v;
            if (hp < 0.f || hp > 100000.f) hp = 100.f;
        }

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

    std::string GetName(Il2CppObject* p) {
        if (!p || !M_get_name) return "?";
        Il2CppObject* r = SafeInvoke(M_get_name, (void*)p, nullptr);
        if (!r) return "?";
        return IL2CPP::ToStr((Il2CppString*)r);
    }

    Il2CppObject* FindBone(Il2CppObject* t, const char* needle, int depth) {
        if (!t || depth > 8 || !M_GetChild || !M_get_childCount) return nullptr;

        Il2CppObject* nr = SafeInvoke(M_get_name, (void*)t, nullptr);
        if (nr) {
            std::string n = IL2CPP::ToStr((Il2CppString*)nr);
            if (!n.empty() && strstr(n.c_str(), needle)) return t;
        }

        Il2CppObject* cco = SafeInvoke(M_get_childCount, (void*)t, nullptr);
        int* ccv = cco ? (int*)SafeUnbox(cco) : nullptr;
        int cc = ccv ? *ccv : 0;
        if (cc > 200) cc = 200;
        for (int i = 0; i < cc; i++) {
            void* a[1] = { (void*)&i };
            Il2CppObject* c = SafeInvoke(M_GetChild, (void*)t, a);
            Il2CppObject* r = FindBone(c, needle, depth + 1);
            if (r) return r;
        }
        return nullptr;
    }

    Il2CppObject* FindBoneExato(Il2CppObject* t, const char* nome, int depth, Il2CppObject** loose) {
        if (!t || depth > 8 || !M_GetChild || !M_get_childCount) return nullptr;

        Il2CppObject* nr = SafeInvoke(M_get_name, (void*)t, nullptr);
        if (nr) {
            std::string n = IL2CPP::ToStr((Il2CppString*)nr);
            if (n == nome) {
                static void* logged[16] = { 0 };
                static int   nLogged = 0;
                bool jaVi = false;
                for (int k = 0; k < nLogged; k++) if (logged[k] == (void*)t) { jaVi = true; break; }
                if (!jaVi && nLogged < 16) {
                    logged[nLogged++] = (void*)t;
                    Log("FindBoneExato: '%s' OK addr=%p", n.c_str(), (void*)t);
                }
                return t;
            }
            if (loose && !*loose && !n.empty()) {
                std::string low = n, alvo = nome;
                for (auto& c : low)  if (c >= 'A' && c <= 'Z') c += 32;
                for (auto& c : alvo) if (c >= 'A' && c <= 'Z') c += 32;
                if (low.find(alvo) != std::string::npos) *loose = t;
            }
        }

        Il2CppObject* cco = SafeInvoke(M_get_childCount, (void*)t, nullptr);
        int* ccv = cco ? (int*)SafeUnbox(cco) : nullptr;
        int cc = ccv ? *ccv : 0;
        if (cc > 200) cc = 200;
        for (int i = 0; i < cc; i++) {
            void* a[1] = { (void*)&i };
            Il2CppObject* c = SafeInvoke(M_GetChild, (void*)t, a);
            Il2CppObject* r = FindBoneExato(c, nome, depth + 1, loose);
            if (r) return r;
        }
        return nullptr;
    }

    Il2CppObject* CamTfRaw() {
        Il2CppObject* lp = LocalPlayer();
        if (!lp || !O_camTf) return nullptr;
        return (Il2CppObject*)SafeReadObjPub(lp, O_camTf);
    }
    Il2CppObject* CamTransform() { return CamTfRaw(); }

    Il2CppObject* CameraObj() {
        Il2CppObject* lp = LocalPlayer();
        if (lp) {
            if (O_camField) {
                Il2CppObject* c = (Il2CppObject*)SafeReadObjPub(lp, O_camField);
                if (c) return c;
            }
            Il2CppObject* tf = CamTfRaw();
            if (tf && kCamera) {
                Il2CppObject* c = GetComponentType(tf, kCamera);
                if (c) return c;
            }
        }
        Il2CppObject* c = MainCamera();
        if (c) return c;

        if (M_get_allCameras) {
            Il2CppObject* arr = SafeInvoke(M_get_allCameras, nullptr, nullptr);
            if (arr) {
                int cnt = ArrLen(arr);
                Vec3 lp3 = LocalPos();
                Il2CppObject* best = nullptr; float bd = 1e9f;
                for (int i = 0; i < cnt; i++) {
                    Il2CppObject* cc = ArrAt(arr, i);
                    if (!cc) continue;
                    Il2CppObject* t = GetTransform(cc);
                    if (!t) continue;
                    float d = GetPos(t).dist(lp3);
                    if (d < bd) { bd = d; best = cc; }
                }
                if (best) return best;
            }
        }
        return M_get_current ? SafeInvoke(M_get_current, nullptr, nullptr) : nullptr;
    }
    Il2CppObject* FindFirstCamera() { return CameraObj(); }

    Vec3 LocalPos() {
        static Vec3 cached;
        static ULONGLONG t = 0;
        ULONGLONG now = GetTickCount64();
        if (now - t < 200) return cached;
        t = now;
        Il2CppObject* lp = LocalPlayer();
        if (lp) cached = GetPos(GetTransform(lp));
        return cached;
    }

    bool WorldToScreen(const Vec3& w, Vec2& out) {
        Il2CppObject* cam = CameraObj();
        if (!cam || !M_WorldToScreen) return false;
        Vec3 p = w;
        void* a[1] = { (void*)&p };
        Il2CppObject* r = SafeInvoke(M_WorldToScreen, (void*)cam, a);
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

        float offStart = 1.2f;
        float offEnd = 1.5f;
        if (offStart + offEnd >= d - 0.5f) return true;

        Vec3 start(from.x + nrm.x * offStart, from.y + nrm.y * offStart, from.z + nrm.z * offStart);
        Vec3 end(to.x - nrm.x * offEnd, to.y - nrm.y * offEnd, to.z - nrm.z * offEnd);

        void* a[2] = { (void*)&start, (void*)&end };
        Il2CppObject* r = SafeInvoke(M_Linecast, nullptr, a);
        bool* any = r ? (bool*)SafeUnbox(r) : nullptr;
        if (!any) return true;
        return !(*any);
    }

    Il2CppObject* LocalGun() {
        Il2CppObject* p = LocalPlayer();
        return (p && O_gunView) ? (Il2CppObject*)SafeReadObjPub(p, O_gunView) : nullptr;
    }

    void ScanPlayers() {
        if (!M_FindObjects || !kPlayer || !IL2CPP::class_get_type || !IL2CPP::type_get_object) return;
        const Il2CppType* tp = IL2CPP::class_get_type(kPlayer);
        if (!tp) return;
        Il2CppObject* to = (Il2CppObject*)IL2CPP::type_get_object(tp);
        if (!to) return;
        void* a[1] = { (void*)to };
        Il2CppObject* arr = SafeInvoke(M_FindObjects, nullptr, a);
        if (!arr) return;
        int n = ArrLen(arr);
        dbg_scanCount = n;

        static int loggedCount = -1;
        if (n != loggedCount) {
            loggedCount = n;
            Log("scan: %d objetos", n);
            for (int i = 0; i < n && i < 8; i++) {
                Il2CppObject* q = ArrAt(arr, i);
                Log("  scan[%d]=%p klass=%p nome=%s", i, (void*)q,
                    (q ? (void*)q->klass : nullptr),
                    (q && q->klass) ? SafeClassName(q->klass) : "?");
            }
        }

        Il2CppObject* local = LocalPlayer();
        for (int i = 0; i < n; i++) {
            Il2CppObject* p = ArrAt(arr, i);
            if (!p || p == local) continue;
            AddTarget(p, true);
        }
    }
} // namespace SDK

static void FillName(std::string& dst, Il2CppObject* p) {
    std::string n = SDK::GetName(p);
    if (!n.empty()) dst = n;
}

void AddTarget(Il2CppObject* p, bool trust) {
    if (!p || !p->klass) return;
    if (!trust && !SDK::IsA(p, SDK::kPlayer)) { SDK::dbg_reject++; return; }

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
            if (t.klass != (void*)p->klass) {
                t.klass = (void*)p->klass;
                t.headBone = nullptr;
                t.footBone = nullptr;
                t.name.clear();
            }
            t.player = p;

            step = 3;
            t.hpMax = 100.f;
            t.hp = SDK::GetHealthPct(p);
            t.health = (int)(t.hp + 0.5f);
            t.alive = (t.hp > 0.5f) && SDK::ReadAliveField(p, true);

            step = 4;
            Il2CppObject* tf = SDK::GetTransform(p);
            step = 5;
            if (tf) {
                if (!t.headBone) t.headBone = SDK::FindBoneExato(tf, "Head", 0, nullptr);
                if (!t.headBone) t.headBone = SDK::FindBoneExato(tf, "head", 0, nullptr);
                if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "Foot", 0, nullptr);
                if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "foot", 0, nullptr);
                if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "Ankle", 0, nullptr);
                if (!t.footBone) t.footBone = SDK::FindBoneExato(tf, "Toe", 0, nullptr);

                step = 7;
                Vec3 base = SDK::GetPos(tf);
                float H = C.espBodyH;
                if (H < 0.5f || H > 3.f) H = 1.70f;

                Vec3 head, feet;
                if (t.headBone && t.footBone) {
                    head = SDK::GetPos(t.headBone);
                    feet = SDK::GetPos(t.footBone);
                }
                else if (t.headBone) {
                    head = SDK::GetPos(t.headBone);
                    feet = head - Vec3(0.f, H, 0.f);
                }
                else if (t.footBone) {
                    feet = SDK::GetPos(t.footBone);
                    head = feet + Vec3(0.f, H, 0.f);
                }
                else {
                    head = base;
                    feet = base - Vec3(0.f, H, 0.f);
                }
                float wh = head.dist(feet);
                if (wh < 0.6f || wh > 3.2f) feet = head - Vec3(0.f, H, 0.f);

                t.head = head;
                t.feet = feet;

                step = 8;
                t.team = SDK::GetTeam(p);
                step = 9;
                t.dist = SDK::LocalPos().dist(t.feet);
                step = 10;
                if (t.name.empty() || strcmp(t.name.c_str(), "?") == 0) FillName(t.name, p);
            }
            step = 11;
            t.seen = GetTickCount64();
            SDK::dbg_adds++;
            step = 12;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static ULONGLONG l = 0;
        if (GetTickCount64() - l > 2000) {
            l = GetTickCount64();
            Log("!!! EXCECAO em AddTarget, passo %d (p=%p)", (int)step, (void*)p);
        }
    }
    LeaveCriticalSection(&g_targetCs);
}

void PruneTargets() {
    uint64_t now = GetTickCount64();
    EnterCriticalSection(&g_targetCs);
    int w = 0;
    for (int i = 0; i < g_targetCount; i++) {
        if (now - g_targets[i].seen > 400) continue;
        if (w != i) g_targets[w] = g_targets[i];
        w++;
    }
    g_targetCount = w;
    LeaveCriticalSection(&g_targetCs);
}