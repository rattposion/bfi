// ============================================================================
//  sdk.cpp — UTF-16->UTF-8, SyncVar, bones, LOS via Physics.Raycast
//
//  >>> CORRECOES acumuladas:
//   1) FindGunStatsObj lia o TAMANHO do array em 0x10 ('bounds'). Fica em 0x18.
//   2) se_runtime_invoke garante il2cpp_thread_attach por thread.
//   3) il2cpp_class_from_type ganhou fallback para o nome longo do export.
//   4) ReadTeam retorna -1 quando NAO conseguiu ler.
//   5) O_rotationIsQuat deixou de ser hardcoded.
//   6) FovRadiusPx centralizado.
//   7) g_frameId nunca era incrementado -> cache de TargetVisible preso.
//   8) Offsets do gunStats resolvidos PELA CLASSE DA INSTANCIA.
//   9) Slots de alvo estaveis + cache de bones por player.
//  10) LOS em round-robin (3 alvos/frame) fora do critical section.
//  11) ReadHealth/ReadTeam nao leem bytes de ponteiro como float.
//  12) LOS identifica o dono do collider por ANCESTRALIDADE (nao por posicao).
//  13) Arma equipada detectada por gunObject.activeInHierarchy.
//  14) AntiHack: FindClassExact (o match de sufixo casava StoreAntiHack).
//  15) FireBullet: direcao = ULTIMO Vector3, nao indice fixo.
//  16) WorldToScreen: 'm' podia sair nao inicializado se as 64 tentativas do
//      guard caissem todas no ramo impar do g_vpSeq (C6001).
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include "il2cpp.h"
#include "il2cpp_api.h"
#include "common.h"
#include "memory.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"

void Log(const char* fmt, ...);
extern HMODULE g_hModule;
const char* g_phase = "boot";

extern "C" {
    Il2CppDomain* (*il2cpp_domain_get)() = nullptr;
    const Il2CppAssembly** (*il2cpp_domain_get_assemblies)(const Il2CppDomain*, size_t*) = nullptr;
    Il2CppImage* (*il2cpp_assembly_get_image)(const Il2CppAssembly*) = nullptr;
    size_t(*il2cpp_image_get_class_count)(const Il2CppImage*) = nullptr;
    Il2CppClass* (*il2cpp_image_get_class)(const Il2CppImage*, size_t) = nullptr;
    const char* (*il2cpp_class_get_name)(const Il2CppClass*) = nullptr;
    const char* (*il2cpp_class_get_namespace)(const Il2CppClass*) = nullptr;
    Il2CppClass* (*il2cpp_class_get_parent)(const Il2CppClass*) = nullptr;
    Il2CppImage* (*il2cpp_class_get_image)(const Il2CppClass*) = nullptr;
    Il2CppType* (*il2cpp_class_get_type)(const Il2CppClass*) = nullptr;
    Il2CppClass* (*il2cpp_class_from_type)(const Il2CppType*) = nullptr;
    FieldInfo* (*il2cpp_class_get_field_from_name)(const Il2CppClass*, const char*) = nullptr;
    FieldInfo* (*il2cpp_class_get_fields)(const Il2CppClass*, void**) = nullptr;
    MethodInfo* (*il2cpp_class_get_method_from_name)(const Il2CppClass*, const char*, int) = nullptr;
    const MethodInfo* (*il2cpp_class_get_methods)(const Il2CppClass*, void**) = nullptr;
    size_t(*il2cpp_field_get_offset)(const FieldInfo*) = nullptr;
    const char* (*il2cpp_field_get_name)(const FieldInfo*) = nullptr;
    Il2CppType* (*il2cpp_field_get_type)(const FieldInfo*) = nullptr;
    void (*il2cpp_field_static_get_value)(const FieldInfo*, void*) = nullptr;
    const char* (*il2cpp_method_get_name)(const MethodInfo*) = nullptr;
    uint32_t(*il2cpp_method_get_param_count)(const MethodInfo*) = nullptr;
    const Il2CppType* (*il2cpp_method_get_param)(const MethodInfo*, uint32_t) = nullptr;
    Il2CppClass* (*il2cpp_object_get_class)(const Il2CppObject*) = nullptr;
    Il2CppObject* (*il2cpp_object_new)(const Il2CppClass*) = nullptr;
    void* (*il2cpp_object_unbox)(const Il2CppObject*) = nullptr;
    Il2CppObject* (*il2cpp_runtime_invoke)(const MethodInfo*, void*, void**, Il2CppException**) = nullptr;
    Il2CppString* (*il2cpp_string_new)(const char*) = nullptr;
    const char* (*il2cpp_string_chars)(const Il2CppString*) = nullptr;
    uint32_t(*il2cpp_array_length)(const Il2CppArray*) = nullptr;
    Il2CppArray* (*il2cpp_array_new)(const Il2CppClass*, size_t) = nullptr;
    Il2CppDomain* (*il2cpp_thread_attach)(const Il2CppDomain*) = nullptr;
    Il2CppObject* (*il2cpp_type_get_object)(const Il2CppType*) = nullptr;
    char* (*il2cpp_type_get_name)(const Il2CppType*) = nullptr;

    Il2CppType* (*il2cpp_method_get_return_type)(const MethodInfo*) = nullptr;
}

namespace IL2CPP_API {
    static int g_resolved = 0, g_missing = 0;
    bool Resolve() {
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");
        if (!ga) { Log("[il2cpp] GameAssembly.dll nao carregada"); return false; }
        struct Entry { const char* name; void** slot; };
        static const Entry tbl[] = {
            {"il2cpp_domain_get", (void**)&il2cpp_domain_get},
            {"il2cpp_domain_get_assemblies", (void**)&il2cpp_domain_get_assemblies},
            {"il2cpp_assembly_get_image", (void**)&il2cpp_assembly_get_image},
            {"il2cpp_image_get_class_count", (void**)&il2cpp_image_get_class_count},
            {"il2cpp_image_get_class", (void**)&il2cpp_image_get_class},
            {"il2cpp_class_get_name", (void**)&il2cpp_class_get_name},
            {"il2cpp_class_get_namespace", (void**)&il2cpp_class_get_namespace},
            {"il2cpp_class_get_parent", (void**)&il2cpp_class_get_parent},
            {"il2cpp_class_get_image", (void**)&il2cpp_class_get_image},
            {"il2cpp_class_get_type", (void**)&il2cpp_class_get_type},
            {"il2cpp_class_from_type", (void**)&il2cpp_class_from_type},
            {"il2cpp_class_get_field_from_name", (void**)&il2cpp_class_get_field_from_name},
            {"il2cpp_class_get_fields", (void**)&il2cpp_class_get_fields},
            {"il2cpp_class_get_method_from_name", (void**)&il2cpp_class_get_method_from_name},
            {"il2cpp_class_get_methods", (void**)&il2cpp_class_get_methods},
            {"il2cpp_field_get_offset", (void**)&il2cpp_field_get_offset},
            {"il2cpp_field_get_name", (void**)&il2cpp_field_get_name},
            {"il2cpp_field_get_type", (void**)&il2cpp_field_get_type},
            {"il2cpp_field_static_get_value", (void**)&il2cpp_field_static_get_value},
            {"il2cpp_method_get_name", (void**)&il2cpp_method_get_name},
            {"il2cpp_method_get_param_count", (void**)&il2cpp_method_get_param_count},
            {"il2cpp_method_get_param", (void**)&il2cpp_method_get_param},
            {"il2cpp_method_get_return_type", (void**)&il2cpp_method_get_return_type},
            {"il2cpp_object_get_class", (void**)&il2cpp_object_get_class},
            {"il2cpp_object_new", (void**)&il2cpp_object_new},
            {"il2cpp_object_unbox", (void**)&il2cpp_object_unbox},
            {"il2cpp_runtime_invoke", (void**)&il2cpp_runtime_invoke},
            {"il2cpp_string_new", (void**)&il2cpp_string_new},
            {"il2cpp_string_chars", (void**)&il2cpp_string_chars},
            {"il2cpp_array_length", (void**)&il2cpp_array_length},
            {"il2cpp_array_new", (void**)&il2cpp_array_new},
            {"il2cpp_thread_attach", (void**)&il2cpp_thread_attach},
            {"il2cpp_type_get_object", (void**)&il2cpp_type_get_object},
            {"il2cpp_type_get_name", (void**)&il2cpp_type_get_name}
        };
        g_resolved = g_missing = 0;
        for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); ++i) {
            FARPROC p = GetProcAddress(ga, tbl[i].name);
            if (p) { *tbl[i].slot = (void*)p; ++g_resolved; }
            else { *tbl[i].slot = nullptr; ++g_missing; Log("[il2cpp] faltando: %s", tbl[i].name); }
        }

        if (!il2cpp_class_from_type) {
            FARPROC p = GetProcAddress(ga, "il2cpp_class_from_il2cpp_type");
            if (p) {
                il2cpp_class_from_type = (decltype(il2cpp_class_from_type))p;
                Log("[il2cpp] class_from_type resolvido pelo nome longo");
            }
        }

        Log("[il2cpp] resolvidas %d, faltando %d", g_resolved, g_missing);
        return g_resolved > 0;
    }
}

#define SE_BEGIN __try
#define SE_END   __except (EXCEPTION_EXECUTE_HANDLER)

static void StrToUtf8(Il2CppString* s, char* out, int outSize) {
    if (!out || outSize < 2) return;
    out[0] = 0;
    if (!s) return;
    SE_BEGIN{
        int len = *(int*)((uintptr_t)s + 0x10);
        if (len <= 0) { out[0] = 0; }
        else {
            int maxc = outSize - 1;
            if (len > maxc) len = maxc;
            const wchar_t* src = (const wchar_t*)((uintptr_t)s + 0x14);
            int n = WideCharToMultiByte(CP_UTF8, 0, src, len, out, maxc, nullptr, nullptr);
            out[(n > 0 && n < outSize) ? n : 0] = 0;
        }
    }
    SE_END{ out[0] = 0; }
}

static Il2CppDomain* se_domain_get() {
    Il2CppDomain* r = nullptr;
    SE_BEGIN{ if (il2cpp_domain_get) r = il2cpp_domain_get(); } SE_END{ r = nullptr; }
    return r;
}
static const Il2CppAssembly** se_domain_get_assemblies(Il2CppDomain* d, size_t* n) {
    const Il2CppAssembly** r = nullptr;
    SE_BEGIN{ if (il2cpp_domain_get_assemblies) r = il2cpp_domain_get_assemblies(d, n); } SE_END{ r = nullptr; }
    return r;
}
static Il2CppImage* se_assembly_get_image(const Il2CppAssembly* a) {
    Il2CppImage* r = nullptr;
    SE_BEGIN{ if (il2cpp_assembly_get_image) r = il2cpp_assembly_get_image(a); } SE_END{ r = nullptr; }
    return r;
}
static size_t se_image_class_count(Il2CppImage* img) {
    size_t r = 0;
    SE_BEGIN{ if (il2cpp_image_get_class_count) r = il2cpp_image_get_class_count(img); } SE_END{ r = 0; }
    return r;
}
static Il2CppClass* se_image_get_class(Il2CppImage* img, size_t i) {
    Il2CppClass* r = nullptr;
    SE_BEGIN{ if (il2cpp_image_get_class) r = il2cpp_image_get_class(img, i); } SE_END{ r = nullptr; }
    return r;
}
static const char* se_class_name(Il2CppClass* k) {
    const char* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_name) r = il2cpp_class_get_name(k); } SE_END{ r = nullptr; }
    return r;
}
static const char* se_class_namespace(Il2CppClass* k) {
    const char* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_namespace) r = il2cpp_class_get_namespace(k); } SE_END{ r = nullptr; }
    return r;
}
static Il2CppClass* se_class_parent(Il2CppClass* k) {
    Il2CppClass* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_parent) r = il2cpp_class_get_parent(k); } SE_END{ r = nullptr; }
    return r;
}
static MethodInfo* se_class_get_method_from_name(Il2CppClass* k, const char* n, int a) {
    MethodInfo* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_method_from_name) r = il2cpp_class_get_method_from_name(k, n, a); } SE_END{ r = nullptr; }
    return r;
}
static FieldInfo* se_class_get_field_from_name(Il2CppClass* k, const char* n) {
    FieldInfo* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_field_from_name) r = il2cpp_class_get_field_from_name(k, n); } SE_END{ r = nullptr; }
    return r;
}
static FieldInfo* se_class_get_fields(Il2CppClass* k, void** iter) {
    FieldInfo* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_fields) r = il2cpp_class_get_fields(k, iter); } SE_END{ r = nullptr; }
    return r;
}
static const char* se_field_name(FieldInfo* f) {
    const char* r = nullptr;
    SE_BEGIN{ if (il2cpp_field_get_name) r = il2cpp_field_get_name(f); } SE_END{ r = nullptr; }
    return r;
}
static size_t se_field_offset(FieldInfo* f) {
    size_t r = 0;
    SE_BEGIN{ if (il2cpp_field_get_offset) r = il2cpp_field_get_offset(f); } SE_END{ r = 0; }
    return r;
}
static void se_field_static_get_value(FieldInfo* f, void* out) {
    SE_BEGIN{ if (il2cpp_field_static_get_value) il2cpp_field_static_get_value(f, out); } SE_END{}
}
static Il2CppClass* se_object_get_class(Il2CppObject* o) {
    if (!o) return nullptr;
    Il2CppClass* r = nullptr;
    SE_BEGIN{
        auto fn = il2cpp_object_get_class;
        if (fn) r = fn(o);
    } SE_END{ r = nullptr; }
    return r;
}
static Il2CppObject* se_object_new(Il2CppClass* k) {
    Il2CppObject* r = nullptr;
    SE_BEGIN{ if (il2cpp_object_new) r = il2cpp_object_new(k); } SE_END{ r = nullptr; }
    return r;
}
static void* se_object_unbox(Il2CppObject* o) {
    void* r = nullptr;
    SE_BEGIN{ if (il2cpp_object_unbox) r = il2cpp_object_unbox(o); } SE_END{ r = nullptr; }
    return r;
}

// ---------------------------------------------------------------------------
//  >>> FIX: attach preguicoso por thread.
// ---------------------------------------------------------------------------
static thread_local bool t_il2cppAttached = false;
static void EnsureThreadAttached() {
    if (t_il2cppAttached) return;
    t_il2cppAttached = true;
    if (!il2cpp_thread_attach || !il2cpp_domain_get) return;

    Il2CppDomain* d = nullptr;
    SE_BEGIN{ d = il2cpp_domain_get(); } SE_END{ d = nullptr; }
    if (!d) return;
    SE_BEGIN{ il2cpp_thread_attach(d); } SE_END{}
}

static Il2CppObject* se_runtime_invoke(const MethodInfo* m, void* o, void** args) {
    if (!m) return nullptr;
    EnsureThreadAttached();
    Il2CppObject* r = nullptr;
    SE_BEGIN{ if (il2cpp_runtime_invoke) r = il2cpp_runtime_invoke(m, o, args, nullptr); } SE_END{ r = nullptr; }
    return r;
}
static Il2CppDomain* se_thread_attach(Il2CppDomain* d) {
    Il2CppDomain* r = nullptr;
    SE_BEGIN{ if (il2cpp_thread_attach) r = il2cpp_thread_attach(d); } SE_END{ r = nullptr; }
    return r;
}
static Il2CppObject* se_type_get_object(Il2CppType* t) {
    Il2CppObject* r = nullptr;
    SE_BEGIN{ if (il2cpp_type_get_object) r = il2cpp_type_get_object(t); } SE_END{ r = nullptr; }
    return r;
}
static Il2CppType* se_class_get_type(Il2CppClass* k) {
    Il2CppType* r = nullptr;
    SE_BEGIN{ if (il2cpp_class_get_type) r = il2cpp_class_get_type(k); } SE_END{ r = nullptr; }
    return r;
}
static bool se_read_bytes(void* src, void* dst, size_t len) {
    SE_BEGIN{ memcpy(dst, src, len); } SE_END{ return false; }
    return true;
}
static bool se_is_exec(void* p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
static void* se_array_at(Il2CppArray* a, uint32_t i) {
    void* r = nullptr;
    SE_BEGIN{ r = *(void**)((uintptr_t)a + 0x20 + (size_t)i * sizeof(void*)); } SE_END{ r = nullptr; }
    return r;
}
static bool se_read_ptr(void* base, size_t off, void** out) {
    *out = nullptr;
    SE_BEGIN{ *out = *(void**)((uintptr_t)base + off); } SE_END{ *out = nullptr; return false; }
    return *out != nullptr;
}
static bool se_read_int(void* base, size_t off, int* out) {
    *out = 0;
    SE_BEGIN{ *out = *(int*)((uintptr_t)base + off); } SE_END{ *out = 0; return false; }
    return true;
}
static bool se_read_bool(void* base, size_t off) {
    unsigned char b = 0;
    SE_BEGIN{ b = *(unsigned char*)((uintptr_t)base + off); } SE_END{ b = 0; }
    return b != 0;
}
static float se_read_float(void* base, size_t off) {
    float r = 0.f;
    SE_BEGIN{ r = *(float*)((uintptr_t)base + off); } SE_END{ r = 0.f; }
    return r;
}
static bool se_read_vec3(void* base, size_t off, Vec3* out) {
    SE_BEGIN{ memcpy(out, (void*)((uintptr_t)base + off), sizeof(Vec3)); } SE_END{ return false; }
    return true;
}

namespace SDK {

    std::unordered_map<std::string, Il2CppClass*> g_classes;
    std::mutex g_classesMx;

    Il2CppObject* g_camera = nullptr;
    Il2CppObject* g_local = nullptr;
    Il2CppObject* g_localPlayer = nullptr;
    Il2CppObject* g_aimTarget = nullptr;

    Target g_targets[MAX_TARGETS];
    int    g_targetCount = 0;
    int    g_localTeam = 0;
    int    dbg_drawn = 0;
    Vec3   g_localPos{ 0.f, 0.f, 0.f };
    CRITICAL_SECTION g_targetCs;
    uint64_t g_frameId = 0;

    std::atomic<int>  g_fireDirSlot{ -1 };
    std::atomic<bool> g_recoilHookActive{ false };
    bool g_rateIsRpm = false;

    bool  g_viewProjOk = false;
    float g_viewProj[16] = { 0 };
    static volatile LONG g_vpSeq = 0;

    uint32_t O_health = 0, O_alive = 0, O_team = 0, O_isLocal = 0, O_name = 0, O_weapon = 0;
    uint32_t O_vel = 0, O_speed = 0, O_rot = 0, O_rotation = 0, O_cam = 0, O_camTf = 0;
    uint32_t O_mCamera = 0, O_humanMeshAnimator = 0, O_humanMeshReferences = 0;
    uint32_t O_stats = 0, O_gunView = 0, O_gunStats = 0, O_gunStatsOnView = 0;
    uint32_t O_curAmmo = 0, O_maxAmmo = 0, O_reloading = 0, O_spread = 0, O_fireRate = 0, O_dmg = 0;
    static uint32_t O_localPlayerScript = 0;
    bool     O_rotationIsQuat = true;

    static bool g_rotIsSyncVar = false;
    static bool g_healthIsSync = false;
    static bool g_teamIsSync = false;
    static Quat g_savedPlayerRot{ 0.f, 0.f, 0.f, 1.f };

    // offsets do gunStats + campo do array de loadout
    static uint32_t O_gsMagSize = 0, O_gsAmmo = 0, O_gsGunObj = 0;
    static uint32_t O_gunStatsArray = 0;
    static Il2CppClass* g_gsCls = nullptr;

    Il2CppClass* kEntity = nullptr;
    Il2CppClass* kPlayerScript = nullptr;
    Il2CppClass* kPlayer = nullptr;
    Il2CppClass* kNPC = nullptr;
    Il2CppClass* kTransform = nullptr;
    Il2CppClass* kCamera = nullptr;
    Il2CppClass* kGunStats = nullptr;
    Il2CppClass* kVector3 = nullptr;
    Il2CppClass* kQuaternion = nullptr;
    Il2CppClass* kAntiHack = nullptr;

    MethodInfo* M_shoot = nullptr;
    MethodInfo* M_recoil = nullptr;
    MethodInfo* M_fireBullet = nullptr;
    MethodInfo* M_FireBullet = nullptr;
    MethodInfo* M_getTransform = nullptr;
    MethodInfo* M_w2s = nullptr;
    MethodInfo* M_lookRot = nullptr;
    MethodInfo* M_set_position = nullptr;
    MethodInfo* M_set_forward = nullptr;
    MethodInfo* M_get_fov = nullptr;
    MethodInfo* M_set_fov = nullptr;

    static MethodInfo* M_get_transform = nullptr;
    static MethodInfo* M_get_name = nullptr;
    static MethodInfo* M_get_childCount = nullptr;
    static MethodInfo* M_GetChild = nullptr;
    static MethodInfo* M_GetComponent = nullptr;
    static MethodInfo* M_camW2C = nullptr;
    static MethodInfo* M_camProj = nullptr;
    static MethodInfo* M_findFromId = nullptr;
    static Il2CppClass* kObject = nullptr;

    static Il2CppClass* kPhysics = nullptr;
    static Il2CppClass* kRaycastHit = nullptr;
    static MethodInfo* M_raycast = nullptr;
    static int          g_rayKind = 0;   // ver kRayNames
    static bool         g_rayUsesRay = false;
    static uint32_t     O_hitDist = 0;
    static uint32_t     O_hitCollider = 0;
    static bool         g_hitColIsId = true;
    static bool         g_losLogged = false;
    static uint8_t      g_losState[MAX_TARGETS];
    static uint64_t     g_losFrame = (uint64_t)-1;

    static const char* kRayNames[] = {
        "off", "Raycast4", "Raycast5", "Linecast3", "Linecast4", "RaycastRay3", "RaycastRay4"
    };

    static std::mutex g_tfMx;
    static std::unordered_map<Il2CppObject*, Il2CppObject*> g_tfCache;
    static std::unordered_map<Il2CppClass*, MethodInfo*>     g_getPosMx;

    static std::vector<uint32_t> g_strOffs;
    static uint32_t g_pickedNameOff = 0;

    static const int kMaxTargets = MAX_TARGETS;

    // slots estaveis de alvo (nao embaralham a cada refresh)
    static Il2CppObject* g_slotOwner[MAX_TARGETS] = {};
    static uint32_t      g_slotSeen[MAX_TARGETS] = {};
    static uint32_t      g_slotGen = 1;

    // cache de bones por player (evita varrer a hierarquia a cada 200ms)
    struct BoneCacheEnt { Il2CppObject* player; Il2CppObject* head; Il2CppObject* foot; };
    static BoneCacheEnt g_boneCache[64] = {};
    static int          g_boneEvict = 0;

    // ======================= pick de campo =======================
    static uint32_t FindTypedField(Il2CppClass* k, std::initializer_list<const char*> names,
        const char* typeSub, const char** outName = nullptr) {
        if (!k) return 0;
        for (Il2CppClass* c = k; c; c = se_class_parent(c)) {
            for (const char* n : names) {
                FieldInfo* f = se_class_get_field_from_name(c, n);
                if (!f) continue;
                if (typeSub) {
                    const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
                    const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                    if (!tn || !strstr(tn, typeSub)) continue;
                }
                if (outName) *outName = n;
                uint32_t o = (uint32_t)se_field_offset(f);
                if (o) return o;
            }
        }
        return 0;
    }

    uint32_t PickFieldOff(Il2CppClass* k, std::initializer_list<const char*> names,
        const char* typeSub) {
        return FindTypedField(k, names, typeSub);
    }

    static const char* FieldTypeOf(Il2CppClass* k, const char* name) {
        FieldInfo* f = k ? se_class_get_field_from_name(k, name) : nullptr;
        if (!f) return "?";
        const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
        const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
        return tn ? tn : "?";
    }

    const char* MethodReturnName(MethodInfo* m) {
        if (!m || !il2cpp_method_get_return_type) return "?";
        const Il2CppType* t = il2cpp_method_get_return_type(m);
        const char* n = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
        return n ? n : "?";
    }

    // ======================= SyncVar (FishNet) =======================
    struct SyncCacheEntry { Il2CppClass* cls; uint32_t off; };
    static SyncCacheEntry g_syncCache[16];
    static int g_syncCacheN = 0;
    static bool g_syncDumped = false;

    static void DumpSyncVarFields(Il2CppClass* k) {
        Log("[sync] ---- fields de %s ----", se_class_name(k));
        void* it = nullptr;
        int n = 0;
        for (;;) {
            FieldInfo* f = se_class_get_fields(k, &it);
            if (!f) break;
            const char* fn = se_field_name(f);
            const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            Log("[sync]   %-22s : %s @0x%X", fn ? fn : "?", tn ? tn : "?", (unsigned)se_field_offset(f));
            if (++n > 40) break;
        }
        Log("[sync] ---- fim ----");
    }

    static uintptr_t SyncVarValueAddr(Il2CppObject* holder, uint32_t off, const char* typeSub) {
        if (!holder || !off) return 0;

        void* p = nullptr;
        if (!se_read_ptr(holder, (size_t)off, &p) || !p) return 0;

        Il2CppObject* obj = (Il2CppObject*)p;
        Il2CppClass* k = se_object_get_class(obj);
        if (!k) return 0;

        const char* kn = se_class_name(k);
        if (!kn || !strstr(kn, "SyncVar")) return 0;

        uint32_t foff = 0;
        for (int i = 0; i < g_syncCacheN; ++i)
            if (g_syncCache[i].cls == k) { foff = g_syncCache[i].off; break; }

        if (!foff) {
            const char* fname = "?";
            foff = FindTypedField(k, { "_value", "value", "Value", "<Value>k__BackingField",
                                       "m_Value", "_currentValue", "currentValue" }, typeSub, &fname);
            if (!foff)
                foff = FindTypedField(k, { "_value", "value", "Value", "<Value>k__BackingField" }, nullptr, &fname);

            if (g_syncCacheN < 16) {
                g_syncCache[g_syncCacheN].cls = k; g_syncCache[g_syncCacheN].off = foff; ++g_syncCacheN;
            }
            Log("[sync] %s -> %s @0x%X (%s)", kn, fname, foff, typeSub);
            if (!g_syncDumped) { g_syncDumped = true; DumpSyncVarFields(k); }
        }
        if (!foff) return 0;
        return (uintptr_t)obj + foff;
    }

    // >>> FIX: quando o SyncVar ainda nao existe, NAO ler os bytes do ponteiro
    //     como float (dava lixo/0). Retorna 100 (desconhecido) nesse caso.
    static float ReadHealth(Il2CppObject* e) {
        if (!e || !O_health) return 100.f;

        float hp;
        uintptr_t a = SyncVarValueAddr(e, O_health, "Single");
        if (a) {
            hp = SafeRead<float>(a, 100.f);
        }
        else if (g_healthIsSync) {
            return 100.f;
        }
        else {
            hp = se_read_float(e, O_health);
        }

        if (!(hp >= 0.f && hp <= 100000.f)) return 100.f;
        if (hp > 0.f && hp < 1.5f) hp *= 100.f;
        return hp;
    }

    // >>> FIX: -1 = "nao consegui ler"
    static int ReadTeam(Il2CppObject* e) {
        if (!e || !O_team) return -1;
        uintptr_t a = SyncVarValueAddr(e, O_team, "Byte");
        if (!a) a = SyncVarValueAddr(e, O_team, "Int32");
        if (a) {
            unsigned char b = SafeRead<unsigned char>(a, 0);
            return (int)b;
        }
        if (g_teamIsSync) return -1;
        return SafeRead<int>((uintptr_t)e + O_team, 0) & 0xFF;
    }

    static bool ReadStringField(Il2CppObject* o, uint32_t off, char* out, int n) {
        if (!o || !off || !out || n <= 0) return false;
        out[0] = 0;

        void* p = nullptr;
        if (!se_read_ptr(o, (size_t)off, &p) || !p) return false;

        uintptr_t a = SyncVarValueAddr(o, off, "String");
        Il2CppString* str = nullptr;
        if (a) {
            void* sp = nullptr;
            if (!se_read_ptr((void*)a, 0, &sp) || !sp) return false;
            str = (Il2CppString*)sp;
        }
        else {
            str = (Il2CppString*)p;
        }
        StrToUtf8(str, out, n);
        return out[0] != 0;
    }

    // ======================= cache de classes =======================
    static void RegisterClass(const char* ns, const char* nm, Il2CppClass* k) {
        if (!k || !nm || !nm[0]) return;
        std::string key;
        if (ns && ns[0]) { key.reserve(strlen(ns) + strlen(nm) + 2); key = ns; key += "."; key += nm; }
        else key = nm;
        std::lock_guard<std::mutex> lk(g_classesMx);
        g_classes.emplace(key, k);
    }

    static void CacheImage(Il2CppImage* img) {
        size_t count = se_image_class_count(img);
        for (size_t i = 0; i < count; ++i) {
            Il2CppClass* k = se_image_get_class(img, i);
            if (!k) continue;
            RegisterClass(se_class_namespace(k), se_class_name(k), k);
        }
    }

    void BuildCache() {
        { std::lock_guard<std::mutex> lk(g_classesMx); g_classes.clear(); }
        Il2CppDomain* dom = se_domain_get();
        if (!dom) { Log("[sdk] cache: domain nulo"); return; }
        size_t n = 0;
        const Il2CppAssembly** asms = se_domain_get_assemblies(dom, &n);
        if (!asms || n == 0) { Log("[sdk] cache: nenhum assembly"); return; }
        Log("[sdk] cache: %d assemblies", (int)n);
        for (size_t a = 0; a < n; ++a) CacheImage(se_assembly_get_image(asms[a]));
        std::lock_guard<std::mutex> lk(g_classesMx);
        Log("[sdk] cache: %d classes", (int)g_classes.size());
    }

    Il2CppClass* FindClass(const char* name, const char* ns) {
        if (!name || !*name) return nullptr;
        std::lock_guard<std::mutex> lk(g_classesMx);
        auto it = g_classes.find(name);
        if (it != g_classes.end()) return it->second;
        if (ns && *ns) {
            std::string key = ns; key += "."; key += name;
            it = g_classes.find(key);
            if (it != g_classes.end()) return it->second;
        }
        const size_t ln = strlen(name);
        for (it = g_classes.begin(); it != g_classes.end(); ++it) {
            const std::string& k = it->first;
            if (k.size() >= ln && k.compare(k.size() - ln, ln, name) == 0) return it->second;
        }
        return nullptr;
    }

    // >>> MELHORIA: match EXATO (sem sufixo).
    static Il2CppClass* FindClassExact(const char* name) {
        if (!name) return nullptr;
        std::lock_guard<std::mutex> lk(g_classesMx);
        auto it = g_classes.find(name);
        return it == g_classes.end() ? nullptr : it->second;
    }

    void ListClassesContaining(const char* sub) {
        if (!sub) return;
        std::lock_guard<std::mutex> lk(g_classesMx);
        int n = 0;
        for (auto& kv : g_classes) {
            if (kv.first.find(sub) == std::string::npos) continue;
            Log("[cls] %s", kv.first.c_str());
            if (++n > 80) break;
        }
        Log("[cls] ---- %d classes contendo \"%s\" ----", n, sub);
    }

    int ScreenW() { return Hooks::g_viewW; }
    int ScreenH() { return Hooks::g_viewH; }

    Il2CppObject* SafeReadObjPub(void* base, unsigned int off) {
        void* r = nullptr;
        if (!se_read_ptr(base, (size_t)off, &r)) return nullptr;
        return (Il2CppObject*)r;
    }

    Il2CppClass* AntiHackClass() { return kAntiHack; }

    Il2CppClass* FindAntiHackClass() {
        static const char* names[] = { "AntiHack", "AntiCheat", "AntiHackSystem", "AntiHackManager" };
        for (const char* n : names) {
            Il2CppClass* k = FindClassExact(n);
            if (k) return k;
        }
        return nullptr;
    }

    Il2CppObject* AntiHackInstance() {
        if (!kAntiHack) return nullptr;
        static const char* names[] = { "instance", "Instance", "_instance", "singleton", "Singleton" };
        for (const char* n : names) {
            FieldInfo* f = se_class_get_field_from_name(kAntiHack, n);
            if (!f) continue;
            Il2CppObject* v = nullptr;
            se_field_static_get_value(f, &v);
            if (v) return v;
        }
        return nullptr;
    }

    // ======================= transform / posicao =======================
    static Il2CppObject* ResolveGetTransform(Il2CppObject* obj) {
        if (!obj) return nullptr;
        Il2CppClass* k = se_object_get_class(obj);
        if (!k) return nullptr;
        if (!M_get_transform) M_get_transform = se_class_get_method_from_name(k, "get_transform", 0);
        if (!M_get_transform) return nullptr;
        return se_runtime_invoke(M_get_transform, obj, nullptr);
    }

    Il2CppObject* GetTransform(void* obj) { return ResolveGetTransform((Il2CppObject*)obj); }

    static Il2CppObject* CachedTransform(Il2CppObject* e) {
        if (!e) return nullptr;
        std::lock_guard<std::mutex> lk(g_tfMx);
        auto it = g_tfCache.find(e);
        if (it != g_tfCache.end()) return it->second;
        Il2CppObject* tr = ResolveGetTransform(e);
        g_tfCache[e] = tr;
        return tr;
    }

    static void ClearTfCache() {
        std::lock_guard<std::mutex> lk(g_tfMx);
        g_tfCache.clear();
    }

    static bool IsTransformClass(Il2CppClass* k) {
        if (!k) return false;
        for (Il2CppClass* c = k; c; c = se_class_parent(c)) {
            const char* n = se_class_name(c);
            if (n && strcmp(n, "Transform") == 0) return true;
        }
        return false;
    }

    static MethodInfo* GetPositionMethod(Il2CppClass* k) {
        if (!k) return nullptr;
        std::lock_guard<std::mutex> lk(g_tfMx);
        auto it = g_getPosMx.find(k);
        if (it != g_getPosMx.end()) return it->second;
        MethodInfo* m = se_class_get_method_from_name(k, "get_position", 0);
        g_getPosMx[k] = m;
        return m;
    }

    Vec3 GetPos(Il2CppObject* obj) {
        Vec3 out{ 0.f, 0.f, 0.f };
        if (!obj) return out;

        Il2CppClass* k = se_object_get_class(obj);
        if (!k) return out;

        Il2CppObject* node = obj;
        if (!IsTransformClass(k)) {
            node = CachedTransform(obj);
            if (!node) return out;
            k = se_object_get_class(node);
            if (!k) return out;
        }

        MethodInfo* m = GetPositionMethod(k);
        if (!m) return out;

        Il2CppObject* boxed = se_runtime_invoke(m, node, nullptr);
        if (!boxed) return out;
        void* p = se_object_unbox(boxed);
        if (p) memcpy(&out, p, sizeof(Vec3));
        return out;
    }

    void SetPos(Il2CppObject* obj, Vec3 v) {
        if (!obj) return;
        Il2CppObject* node = obj;
        Il2CppClass* k = se_object_get_class(node);
        if (!k) return;
        if (!IsTransformClass(k)) {
            node = CachedTransform(obj);
            if (!node) return;
            k = se_object_get_class(node);
            if (!k) return;
        }
        MethodInfo* sp = se_class_get_method_from_name(k, "set_position", 1);
        if (!sp) return;

        Vec3 vv = v;
        void* args[1]; args[0] = &vv;
        se_runtime_invoke(sp, node, args);
    }

    // ======================= rotacao =======================
    Il2CppObject* CamTfObj() {
        if (!g_localPlayer || !O_camTf) return nullptr;
        Il2CppObject* tf = (Il2CppObject*)SafeReadObjPub(g_localPlayer, O_camTf);
        if (!tf) return nullptr;
        if (kTransform && !IsA(tf, kTransform)) return nullptr;
        return tf;
    }

    Quat LookRot(Vec3 dir) {
        Quat out{ 0.f, 0.f, 0.f, 1.f };
        if (!kQuaternion) kQuaternion = FindClass("Quaternion", "UnityEngine");
        if (!kQuaternion) return out;
        if (!M_lookRot) M_lookRot = se_class_get_method_from_name(kQuaternion, "LookRotation", 1);
        if (!M_lookRot) return out;

        Vec3 d = dir;
        void* a[1]; a[0] = &d;
        Il2CppObject* r = se_runtime_invoke(M_lookRot, nullptr, a);
        if (!r) return out;
        void* p = se_object_unbox(r);
        if (p) memcpy(&out, p, sizeof(Quat));
        return out;
    }

    bool GetRot(Il2CppObject* tf, Quat& out) {
        if (!tf) return false;
        Il2CppClass* k = se_object_get_class(tf);
        if (!k) return false;
        MethodInfo* gr = se_class_get_method_from_name(k, "get_rotation", 0);
        if (!gr) return false;
        Il2CppObject* b = se_runtime_invoke(gr, tf, nullptr);
        if (!b) return false;
        void* p = se_object_unbox(b);
        if (!p) return false;
        memcpy(&out, p, sizeof(Quat));
        return true;
    }

    bool SetRot(Il2CppObject* tf, Quat q) {
        if (!tf) return false;
        Il2CppClass* k = se_object_get_class(tf);
        if (!k) return false;
        MethodInfo* sr = se_class_get_method_from_name(k, "set_rotation", 1);
        if (!sr) return false;
        void* a[1]; a[0] = &q;
        se_runtime_invoke(sr, tf, a);
        return true;
    }

    void SetPlayerRot(Il2CppObject* p, const Quat& q) {
        if (!p || !O_rotation) return;
        if (g_rotIsSyncVar) {
            uintptr_t a = SyncVarValueAddr(p, O_rotation, "Quaternion");
            if (a) { SafeWrite(a, (const void*)&q, sizeof(Quat)); return; }
        }
        SafeWrite((uintptr_t)p + O_rotation, (const void*)&q, sizeof(Quat));
    }

    Quat GetPlayerRot(Il2CppObject* p) {
        Quat q{ 0.f, 0.f, 0.f, 1.f };
        if (!p || !O_rotation) return q;
        if (g_rotIsSyncVar) {
            uintptr_t a = SyncVarValueAddr(p, O_rotation, "Quaternion");
            if (a) { se_read_bytes((void*)a, &q, sizeof(Quat)); return q; }
        }
        se_read_bytes((void*)((uintptr_t)p + O_rotation), &q, sizeof(Quat));
        return q;
    }

    Vec3 GetVelocity(Il2CppObject* o) {
        Vec3 v{ 0.f, 0.f, 0.f };
        if (!o || !O_vel) return v;
        se_read_vec3(o, O_vel, &v);
        return v;
    }

    Vec3 GetBonePos(void* bone) { return GetPos((Il2CppObject*)bone); }

    Vec3 MuzzlePos() {
        Il2CppObject* camTf = CamTfObj();
        if (camTf) {
            Vec3 p = GetPos(camTf);
            if (!p.zero()) return p;
        }
        if (g_localPlayer) {
            Vec3 p = GetPos(g_localPlayer);
            return Vec3(p.x, p.y + 1.5f, p.z);
        }
        return Vec3();
    }

    bool IsA(Il2CppObject* o, Il2CppClass* c) {
        if (!o || !c) return false;
        for (Il2CppClass* k = se_object_get_class(o); k; k = se_class_parent(k))
            if (k == c) return true;
        return false;
    }

    float GetHealth(Il2CppObject* o) { return ReadHealth(o); }

    bool IsEnemy(Il2CppObject* o) {
        if (!o) return false;
        if (!O_team) return true;
        int t = ReadTeam(o);
        if (t < 0 || g_localTeam < 0) return true;
        return t != g_localTeam;
    }

    // ======================= nome =======================
    static void BuildStringOffsets() {
        g_strOffs.clear();
        if (!kPlayerScript) return;
        void* it = nullptr;
        for (;;) {
            FieldInfo* f = se_class_get_fields(kPlayerScript, &it);
            if (!f) break;
            const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (!tn || !strstr(tn, "String")) continue;
            uint32_t o = (uint32_t)se_field_offset(f);
            if (o) g_strOffs.push_back(o);
        }
        Log("[sdk] campos String em PlayerScript: %d", (int)g_strOffs.size());
    }

    static bool PlausibleName(const char* s) {
        if (!s || !s[0]) return false;
        size_t len = strlen(s);
        if (len < 2 || len > 24) return false;
        bool allDigits = true;
        for (const char* p = s; *p; ++p) {
            if (*p < 32 || *p > 126) return false;
            if (*p < '0' || *p > '9') allDigits = false;
        }
        return !allDigits;
    }

    void GetNameBuf(Il2CppObject* o, char* out, int n) {
        if (!out || n <= 0) return;
        out[0] = 0;
        if (!o) return;

        char buf[64];

        if (O_name && ReadStringField(o, O_name, buf, sizeof(buf)) && PlausibleName(buf)) {
            strncpy_s(out, (size_t)n, buf, _TRUNCATE);
            return;
        }
        for (uint32_t off : g_strOffs) {
            if (!ReadStringField(o, off, buf, sizeof(buf))) continue;
            if (!PlausibleName(buf)) continue;
            if (!g_pickedNameOff) { g_pickedNameOff = off; Log("[sdk] campo de nome = 0x%X (\"%s\")", off, buf); }
            strncpy_s(out, (size_t)n, buf, _TRUNCATE);
            return;
        }
    }

    std::string GetName(Il2CppObject* o) {
        char buf[64]{};
        GetNameBuf(o, buf, sizeof(buf));
        return std::string(buf);
    }

    Il2CppObject* GetGunStats(Il2CppObject* p) { return (p && O_gunStats) ? SafeReadObjPub(p, O_gunStats) : nullptr; }
    Il2CppObject* GetWeapon(Il2CppObject* p) { return (p && O_weapon) ? SafeReadObjPub(p, O_weapon) : nullptr; }

    // ======================= diag =======================
    static void DumpStringFields(Il2CppObject* inst) {
        if (!inst) return;
        Il2CppClass* k = se_object_get_class(inst);
        if (!k) return;
        Log("[str] ---- campos String de %s ----", se_class_name(k));
        void* iter = nullptr;
        int n = 0;
        char buf[96];
        for (;;) {
            FieldInfo* f = se_class_get_fields(k, &iter);
            if (!f) break;
            const char* fn = se_field_name(f);
            if (!fn) continue;
            const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (!tn || !strstr(tn, "String")) continue;
            uint32_t off = (uint32_t)se_field_offset(f);
            if (!off) continue;
            if (ReadStringField(inst, off, buf, sizeof(buf)))
                Log("[str]   %s @0x%X = \"%s\"", fn, off, buf);
            else
                Log("[str]   %s @0x%X = (vazio)", fn, off);
            if (++n > 25) break;
        }
        Log("[str] ---- fim ----");
    }

    static void LogHierarchy(Il2CppObject* root, int maxNodes) {
        if (!root) { Log("[hier] raiz nula"); return; }
        static Il2CppObject* stk[1024];
        int top = 0;
        stk[top++] = root;
        int visited = 0;

        while (top > 0 && visited < maxNodes) {
            Il2CppObject* cur = stk[--top];
            if (!cur) continue;
            Il2CppClass* k = se_object_get_class(cur);
            if (!k) continue;

            char nm[128] = "?";
            MethodInfo* gn = se_class_get_method_from_name(k, "get_name", 0);
            if (gn) {
                Il2CppObject* s = se_runtime_invoke(gn, cur, nullptr);
                if (s) StrToUtf8((Il2CppString*)s, nm, sizeof(nm));
            }

            int cc = 0;
            MethodInfo* gc = se_class_get_method_from_name(k, "get_childCount", 0);
            if (gc) {
                Il2CppObject* b = se_runtime_invoke(gc, cur, nullptr);
                if (b) { void* p = se_object_unbox(b); if (p) cc = *(int*)p; }
            }

            Log("[hier] %-40s filhos=%d", nm, cc);

            MethodInfo* gch = se_class_get_method_from_name(k, "GetChild", 1);
            if (!gch) continue;

            for (int i = 0; i < cc && top < 1024; ++i) {
                int idx = i;
                void* args[1]; args[0] = &idx;
                Il2CppObject* c = se_runtime_invoke(gch, cur, args);
                if (c) stk[top++] = c;
            }
            ++visited;
        }
    }

    // ======================= bones =======================
    static const char* kHeads[] = { "Head", "head", "HEAD", "Bip01 Head",
                                    "mixamorig:Head", "neck", "Bone_Head" };
    static const char* kFeet[] = { "Foot", "foot", "Foot_L", "Bip01 L Foot",
                                   "mixamorig:LeftFoot", "toe", "Bone_Foot" };

    Il2CppObject* FindBoneContains(Il2CppObject* root, const char* sub, int budget) {
        if (!root || !sub) return nullptr;

        Il2CppClass* rk = se_object_get_class(root);
        if (!rk) return nullptr;
        if (!IsTransformClass(rk)) {
            root = CachedTransform(root);
            if (!root) return nullptr;
        }

        static Il2CppObject* stk[2048];
        int top = 0;
        stk[top++] = root;

        while (top > 0 && budget-- > 0) {
            Il2CppObject* cur = stk[--top];
            if (!cur) continue;
            Il2CppClass* k = se_object_get_class(cur);
            if (!k) continue;

            if (!M_get_name) M_get_name = se_class_get_method_from_name(k, "get_name", 0);
            if (M_get_name) {
                Il2CppObject* s = se_runtime_invoke(M_get_name, cur, nullptr);
                if (s) {
                    char nm[128];
                    StrToUtf8((Il2CppString*)s, nm, sizeof(nm));
                    if (nm[0] && strstr(nm, sub)) return cur;
                }
            }

            if (!M_get_childCount) M_get_childCount = se_class_get_method_from_name(k, "get_childCount", 0);
            if (!M_GetChild) M_GetChild = se_class_get_method_from_name(k, "GetChild", 1);
            if (!M_get_childCount || !M_GetChild) continue;

            int cc = 0;
            Il2CppObject* b = se_runtime_invoke(M_get_childCount, cur, nullptr);
            if (b) { void* p = se_object_unbox(b); if (p) cc = *(int*)p; }

            for (int i = 0; i < cc && top < 2048; ++i) {
                int idx = i;
                void* args[1]; args[0] = &idx;
                Il2CppObject* c = se_runtime_invoke(M_GetChild, cur, args);
                if (c) stk[top++] = c;
            }
        }
        return nullptr;
    }

    static Il2CppObject* FindBoneAny(Il2CppObject* root, const char** names, int count) {
        if (!root) return nullptr;
        for (int i = 0; i < count; ++i) {
            Il2CppObject* b = FindBoneContains(root, names[i], 4096);
            if (b) return b;
        }
        return nullptr;
    }

    // >>> MELHORIA: bones cacheados por player.
    static BoneCacheEnt* BoneCacheFind(Il2CppObject* p) {
        for (auto& b : g_boneCache) if (b.player == p) return &b;
        return nullptr;
    }

    static BoneCacheEnt* BoneCacheInsert(Il2CppObject* p) {
        for (auto& b : g_boneCache) if (!b.player) { b.player = p; b.head = b.foot = nullptr; return &b; }
        BoneCacheEnt* b = &g_boneCache[g_boneEvict++ % 64];
        b->player = p; b->head = b->foot = nullptr;
        return b;
    }

    static void ResolveBones(Target& t, Il2CppObject* e) {
        BoneCacheEnt* bc = BoneCacheFind(e);
        if (!bc) {
            bc = BoneCacheInsert(e);
            if (bc) {
                Il2CppObject* playerTf = CachedTransform(e);
                Il2CppObject* animTf = nullptr;
                if (O_humanMeshAnimator) {
                    Il2CppObject* anim = SafeReadObjPub(e, O_humanMeshAnimator);
                    if (anim) animTf = ResolveGetTransform(anim);
                }

                static bool hierDumped = false;
                if (!hierDumped) {
                    hierDumped = true;
                    Log("[hier] ---- player transform ----");
                    LogHierarchy(playerTf, 80);
                    if (animTf && animTf != playerTf) {
                        Log("[hier] ---- animator transform ----");
                        LogHierarchy(animTf, 80);
                    }
                    Log("[hier] ---- fim ----");
                }

                bc->head = FindBoneAny(playerTf, kHeads, 7);
                bc->foot = FindBoneAny(playerTf, kFeet, 7);
                if ((!bc->head || !bc->foot) && animTf && animTf != playerTf) {
                    if (!bc->head) bc->head = FindBoneAny(animTf, kHeads, 7);
                    if (!bc->foot) bc->foot = FindBoneAny(animTf, kFeet, 7);
                }
                Log("[sdk] bones %p head=%p foot=%p", e, bc->head, bc->foot);
            }
        }
        if (bc) { t.headBone = bc->head; t.footBone = bc->foot; }
    }

    // ======================= camera / view =======================
    Il2CppObject* CameraObj() {
        if (!kCamera) kCamera = FindClass("Camera", "UnityEngine");
        if (!kCamera) return nullptr;

        if (MethodInfo* m = se_class_get_method_from_name(kCamera, "get_main", 0)) {
            Il2CppObject* c = se_runtime_invoke(m, nullptr, nullptr);
            if (c) return c;
        }
        if (g_localPlayer && O_mCamera) {
            Il2CppObject* c = SafeReadObjPub(g_localPlayer, O_mCamera);
            if (c && IsA(c, kCamera)) return c;
        }
        if (g_localPlayer && O_camTf) {
            Il2CppObject* tf = SafeReadObjPub(g_localPlayer, O_camTf);
            if (tf) {
                Il2CppClass* tk = se_object_get_class(tf);
                if (tk) {
                    if (!M_GetComponent) M_GetComponent = se_class_get_method_from_name(tk, "GetComponent", 1);
                    if (M_GetComponent) {
                        Il2CppType* t = se_class_get_type(kCamera);
                        if (t) {
                            Il2CppObject* to = se_type_get_object(t);
                            if (to) {
                                void* a[1]; a[0] = to;
                                Il2CppObject* c = se_runtime_invoke(M_GetComponent, tf, a);
                                if (c) return c;
                            }
                        }
                    }
                }
            }
        }
        if (MethodInfo* m = se_class_get_method_from_name(kCamera, "get_current", 0)) {
            Il2CppObject* c = se_runtime_invoke(m, nullptr, nullptr);
            if (c) return c;
        }
        if (MethodInfo* m = se_class_get_method_from_name(kCamera, "get_allCameras", 0)) {
            Il2CppArray* arr = (Il2CppArray*)se_runtime_invoke(m, nullptr, nullptr);
            if (arr && il2cpp_array_length && il2cpp_array_length(arr) > 0)
                return (Il2CppObject*)se_array_at(arr, 0);
        }
        return nullptr;
    }

    Il2CppObject* GetCamera() { return g_camera; }

    static bool ReadMatrix(Il2CppObject* obj, MethodInfo* m, float out[16]) {
        if (!obj || !m) return false;
        Il2CppObject* boxed = se_runtime_invoke(m, obj, nullptr);
        if (!boxed) return false;
        void* p = se_object_unbox(boxed);
        if (!p) return false;
        return se_read_bytes(p, out, 16 * sizeof(float));
    }

    void UpdateView() {
        g_camera = CameraObj();
        if (!g_camera) { g_viewProjOk = false; return; }

        Il2CppClass* ck = se_object_get_class(g_camera);
        if (!ck) { g_viewProjOk = false; return; }
        if (!M_camW2C)  M_camW2C = se_class_get_method_from_name(ck, "get_worldToCameraMatrix", 0);
        if (!M_camProj) M_camProj = se_class_get_method_from_name(ck, "get_projectionMatrix", 0);

        float V[16] = {}, P[16] = {};
        if (!ReadMatrix(g_camera, M_camW2C, V) || !ReadMatrix(g_camera, M_camProj, P)) {
            g_viewProjOk = false; return;
        }

        float vp[16] = {};
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                float s = 0.f;
                for (int k = 0; k < 4; ++k) s += P[k * 4 + r] * V[c * 4 + k];
                vp[c * 4 + r] = s;
            }

        InterlockedIncrement(&g_vpSeq);
        memcpy(g_viewProj, vp, sizeof(vp));
        g_viewProjOk = true;
        InterlockedIncrement(&g_vpSeq);
    }

    bool WorldToScreen(Vec3 w, Vec2& out) {
        if (!g_viewProjOk) return false;

        // >>> FIX C6001: 'm' podia sair nao inicializado se as 64 tentativas do
        //     guard caissem todas no ramo impar do g_vpSeq.
        float m[16] = {};
        bool copied = false;
        for (int guard = 0; guard < 64; ++guard) {
            LONG s1 = g_vpSeq;
            if (s1 & 1) { YieldProcessor(); continue; }
            memcpy(m, g_viewProj, sizeof(m));
            copied = true;
            if (g_vpSeq == s1) break;
        }
        if (!copied) memcpy(m, g_viewProj, sizeof(m));

        float x = m[0] * w.x + m[4] * w.y + m[8] * w.z + m[12];
        float y = m[1] * w.x + m[5] * w.y + m[9] * w.z + m[13];
        float ww = m[3] * w.x + m[7] * w.y + m[11] * w.z + m[15];
        if (ww <= 0.0001f) return false;

        float inv = 1.f / ww;
        out.x = ((x * inv) * 0.5f + 0.5f) * (float)Hooks::g_viewW;
        out.y = (1.f - ((y * inv) * 0.5f + 0.5f)) * (float)Hooks::g_viewH;
        return out.x == out.x && out.y == out.y;
    }

    bool VisibleLine(Vec3 a, Vec3 b) {
        Vec2 sb;
        if (!WorldToScreen(b, sb)) return false;

        Vec3 a2 = a + (b - a) * 0.01f;
        Vec2 sa;
        if (!WorldToScreen(a2, sa)) {
            sa.x = (float)Hooks::g_viewW * 0.5f;
            sa.y = (float)Hooks::g_viewH * 0.5f;
        }

        const float W = (float)Hooks::g_viewW, H = (float)Hooks::g_viewH;
        auto inside = [&](const Vec2& p) {
            return p.x > -W * 0.25f && p.x < W * 1.25f && p.y > -H * 0.25f && p.y < H * 1.25f;
            };
        return inside(sa) && inside(sb);
    }

    // ======================= LINE OF SIGHT =======================
    // Backends, em ordem de preferencia:
    //   1 Raycast(Vector3,Vector3,out RaycastHit,float)
    //   2 Raycast(Vector3,Vector3,out RaycastHit,float,int)
    //   3 Linecast(Vector3,Vector3,out RaycastHit)
    //   4 Linecast(Vector3,Vector3,out RaycastHit,int)
    //   5 Raycast(Ray,out RaycastHit,float)
    //   6 Raycast(Ray,out RaycastHit,float,int)
    static MethodInfo* FindPhysicsRay(const char* name, int nParams, int kind,
        const char* t0, const char* t1, const char* t2) {
        if (!kPhysics) kPhysics = FindClass("Physics", "UnityEngine");
        if (!kPhysics) return nullptr;

        void* it = nullptr;
        for (;;) {
            const MethodInfo* m = il2cpp_class_get_methods(kPhysics, &it);
            if (!m) break;
            if (strcmp(il2cpp_method_get_name(m), name) != 0) continue;
            if ((int)il2cpp_method_get_param_count(m) != nParams) continue;

            const Il2CppType* p0 = il2cpp_method_get_param(m, 0);
            if (!p0) continue;
            const char* n0 = il2cpp_type_get_name(p0);
            if (!n0 || !strstr(n0, t0)) continue;

            if (t1) {
                const Il2CppType* p1 = il2cpp_method_get_param(m, 1);
                const char* n1 = p1 ? il2cpp_type_get_name(p1) : nullptr;
                if (!n1 || !strstr(n1, t1)) continue;
            }
            if (t2) {
                const Il2CppType* p2 = il2cpp_method_get_param(m, 2);
                const char* n2 = p2 ? il2cpp_type_get_name(p2) : nullptr;
                if (!n2 || !strstr(n2, t2)) continue;
            }
            g_rayKind = kind;
            return (MethodInfo*)m;
        }
        return nullptr;
    }

    static uint32_t HitDistOffset() {
        if (O_hitDist) return O_hitDist;
        if (!kRaycastHit) kRaycastHit = FindClass("RaycastHit", "UnityEngine");
        if (kRaycastHit) {
            FieldInfo* f = se_class_get_field_from_name(kRaycastHit, "m_Distance");
            if (!f) f = se_class_get_field_from_name(kRaycastHit, "distance");
            if (f) O_hitDist = (uint32_t)se_field_offset(f);

            FieldInfo* fc = se_class_get_field_from_name(kRaycastHit, "m_Collider");
            if (!fc) fc = se_class_get_field_from_name(kRaycastHit, "collider");
            if (fc) {
                const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(fc) : nullptr;
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                g_hitColIsId = !tn || (strstr(tn, "Int32") != nullptr) || (strstr(tn, "Int") != nullptr);
                O_hitCollider = (uint32_t)se_field_offset(fc);
            }
        }
        if (!O_hitDist) O_hitDist = 0x1C;
        return O_hitDist;
    }

    static void ResolveRaycast() {
        if (M_raycast) return;
        g_rayKind = 0;
        g_rayUsesRay = false;

        M_raycast = FindPhysicsRay("Raycast", 4, 1, "Vector3", "Vector3", "RaycastHit");
        if (!M_raycast) M_raycast = FindPhysicsRay("Raycast", 5, 2, "Vector3", "Vector3", "RaycastHit");
        if (!M_raycast) M_raycast = FindPhysicsRay("Linecast", 3, 3, "Vector3", "Vector3", "RaycastHit");
        if (!M_raycast) M_raycast = FindPhysicsRay("Linecast", 4, 4, "Vector3", "Vector3", "RaycastHit");
        if (!M_raycast) M_raycast = FindPhysicsRay("Raycast", 3, 5, "Ray", nullptr, "RaycastHit");
        if (!M_raycast) M_raycast = FindPhysicsRay("Raycast", 4, 6, "Ray", nullptr, "RaycastHit");

        if (M_raycast) {
            g_rayUsesRay = (g_rayKind >= 5);
            HitDistOffset();
        }
        if (!g_losLogged) {
            g_losLogged = true;
            Log("[los] Physics=%p metodo=%p backend=%s hitOff=0x%X colOff=0x%X colIsId=%d",
                kPhysics, M_raycast, kRayNames[g_rayKind], O_hitDist, O_hitCollider, (int)g_hitColIsId);
        }
    }

    static MethodInfo* FindRaycast4() { ResolveRaycast(); return M_raycast; }

    bool LosReady() { if (!M_raycast) ResolveRaycast(); return M_raycast != nullptr; }

    const char* LosBackend() {
        int k = g_rayKind;
        if (k < 0 || k > 6) k = 0;
        return kRayNames[k];
    }

    struct RayS { Vec3 origin; Vec3 dir; };

    static bool ShootRay(Vec3 from, Vec3 dir, float len, unsigned char* hitBuf) {
        if (g_rayKind == 0) return false;

        Vec3 sv = from, dv = dir; float lv = len; int mask = -1;
        RayS ray{ from, dir };
        void* args[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

        switch (g_rayKind) {
        case 1: args[0] = &sv;  args[1] = &dv;    args[2] = hitBuf; args[3] = &lv; break;
        case 2: args[0] = &sv;  args[1] = &dv;    args[2] = hitBuf; args[3] = &lv; args[4] = &mask; break;
        case 3: args[0] = &sv;  args[1] = &dv;    args[2] = hitBuf; break;
        case 4: args[0] = &sv;  args[1] = &dv;    args[2] = hitBuf; args[3] = &mask; break;
        case 5: args[0] = &ray; args[1] = hitBuf; args[2] = &lv; break;
        case 6: args[0] = &ray; args[1] = hitBuf; args[2] = &lv; args[3] = &mask; break;
        default: return false;
        }

        Il2CppObject* r = se_runtime_invoke(M_raycast, nullptr, args);
        if (!r) return false;
        void* pb = se_object_unbox(r);
        return pb && *(unsigned char*)pb != 0;
    }

    static Il2CppObject* ObjectFromInstanceID(int id) {
        if (id <= 0) return nullptr;
        if (!kObject) kObject = FindClass("Object", "UnityEngine");
        if (!kObject) return nullptr;
        if (!M_findFromId) M_findFromId = se_class_get_method_from_name(kObject, "FindObjectFromInstanceID", 1);
        if (!M_findFromId) return nullptr;
        void* a[1]; a[0] = &id;
        return se_runtime_invoke(M_findFromId, nullptr, a);
    }

    // --- identificacao do dono do collider por ANCESTRALIDADE ---
    // >>> FIX: a versao anterior comparava a POSICAO do transform do collider
    //     com a camera. O collider do player e filho do root, e o root fica
    //     nos pes (~1.5-2m abaixo do olho), entao "self" nunca casava e o
    //     codigo caia no return false -> quase tudo marcado como oculto.
    static MethodInfo* M_get_parent = nullptr;

    static Il2CppObject* ParentOf(Il2CppObject* tf) {
        if (!tf) return nullptr;
        Il2CppClass* k = se_object_get_class(tf);
        if (!k) return nullptr;
        if (!M_get_parent) M_get_parent = se_class_get_method_from_name(k, "get_parent", 0);
        if (!M_get_parent) return nullptr;
        return se_runtime_invoke(M_get_parent, tf, nullptr);
    }

    static Il2CppObject* ColliderFromHit(unsigned char* hitBuf) {
        if (!O_hitCollider) return nullptr;
        if (g_hitColIsId) {
            int id = 0;
            if (!se_read_int(hitBuf, O_hitCollider, &id) || id <= 0) return nullptr;
            return ObjectFromInstanceID(id);
        }
        void* p = nullptr;
        if (!se_read_ptr(hitBuf, O_hitCollider, &p) || !p) return nullptr;
        return (Il2CppObject*)p;
    }

    // 0 = obstaculo | 1 = corpo do local | 2 = alvo
    static int OwnerKind(Il2CppObject* col, Il2CppObject* localTf,
        Il2CppObject* targetTf, Vec3 aim) {
        Il2CppObject* tf = ResolveGetTransform(col);
        if (!tf) return 0;

        Il2CppObject* walk = tf;
        for (int d = 0; walk && d < 10; ++d) {
            if (localTf && walk == localTf) return 1;
            if (targetTf && walk == targetTf) return 2;
            walk = ParentOf(walk);
        }

        if (targetTf && localTf) return 0;   // ancestralidade resolvida -> obstaculo

        Vec3 op = GetPos(tf);                // fallback quando faltou referencia
        if (op.zero()) return 0;
        if (!targetTf && (op - aim).len() < 2.5f) return 2;
        if (!localTf && (op - g_localPos).len() < 1.5f) return 1;
        return 0;
    }

    bool HasLineOfSightEx(Vec3 eye, Vec3 aim, Il2CppObject* localTf, Il2CppObject* targetTf) {
        if (!M_raycast) ResolveRaycast();
        if (!M_raycast) return true;
        if (!O_hitDist) HitDistOffset();

        if (!localTf && g_localPlayer) localTf = CachedTransform(g_localPlayer);

        Vec3 d = aim - eye;
        float dist = d.len();
        if (dist < 2.0f) return true;
        Vec3 dir{ d.x / dist, d.y / dist, d.z / dist };

        // rede de seguranca para quando o collider nao pode ser resolvido
        float skip = dist * 0.30f;
        if (skip > 1.4f) skip = 1.4f;

        alignas(16) unsigned char hitBuf[160];
        float travelled = 0.f;

        for (int iter = 0; iter < 10; ++iter) {
            float len = dist - travelled;
            if (len < 0.1f) return true;

            memset(hitBuf, 0, sizeof(hitBuf));
            Vec3 sv = eye + dir * travelled;

            if (!ShootRay(sv, dir, len, hitBuf)) return true;   // nada no caminho

            Vec3 hitPoint{ 0.f, 0.f, 0.f };
            se_read_bytes(hitBuf, &hitPoint, sizeof(Vec3));
            float hitDist = (hitPoint - sv).len();
            if (hitDist < 0.01f)
                se_read_bytes(hitBuf + O_hitDist, &hitDist, sizeof(float));

            // classificacao pelo dono real do collider
            if (Il2CppObject* col = ColliderFromHit(hitBuf)) {
                int kind = OwnerKind(col, localTf, targetTf, aim);
                if (kind == 2) return true;                        // bateu no alvo
                if (kind == 1) { travelled += 0.30f; continue; }   // proprio corpo/arma
                return false;                                      // obstaculo real
            }

            // fallback por distancia
            if (hitDist < 0.05f) { travelled += 0.30f; continue; }
            float abs = travelled + hitDist;
            if (abs < skip) { travelled = abs + 0.06f; continue; }
            return abs >= (dist - 0.9f);
        }
        return true;
    }

    bool HasLineOfSight(Vec3 eye, Vec3 aim) {
        return HasLineOfSightEx(eye, aim, nullptr, nullptr);
    }

    bool TargetVisible(int idx, Vec3 eye, Vec3 aim) {
        if (idx < 0 || idx >= MAX_TARGETS) return HasLineOfSight(eye, aim);

        if (g_losFrame != g_frameId) {
            g_losFrame = g_frameId;
            memset(g_losState, 0xFF, sizeof(g_losState));
        }
        uint8_t st = g_losState[idx];
        if (st != 0xFF) return st != 0;

        bool vis = HasLineOfSight(eye, aim);
        g_losState[idx] = vis ? 1 : 0;
        return vis;
    }

    bool RaycastTest(Vec3 from, Vec3 to, float& outHitDist) {
        outHitDist = -1.f;
        if (!M_raycast) ResolveRaycast();
        if (!M_raycast) return false;
        if (!O_hitDist) HitDistOffset();

        Vec3 d = to - from;
        float len = d.len();
        if (len < 0.01f) return false;
        Vec3 dir{ d.x / len, d.y / len, d.z / len };

        alignas(16) unsigned char hitBuf[160];
        memset(hitBuf, 0, sizeof(hitBuf));
        if (!ShootRay(from, dir, len, hitBuf)) return false;

        Vec3 hitPoint{ 0.f, 0.f, 0.f };
        se_read_bytes(hitBuf, &hitPoint, sizeof(Vec3));
        outHitDist = (hitPoint - from).len();
        if (outHitDist < 0.01f)
            se_read_bytes(hitBuf + O_hitDist, &outHitDist, sizeof(float));
        return true;
    }

    float CameraCotHalfFovY() {
        if (!g_camera) return 1.732f;
        Il2CppClass* ck = se_object_get_class(g_camera);
        if (!ck) return 1.732f;
        if (!M_camProj) M_camProj = se_class_get_method_from_name(ck, "get_projectionMatrix", 0);
        float P[16] = {};
        if (!ReadMatrix(g_camera, M_camProj, P)) return 1.732f;
        if (P[5] <= 0.0001f) return 1.732f;
        return P[5];
    }

    // >>> FIX: fonte UNICA do raio do FOV.
    float FovRadiusPx(float deg) {
        float cot = CameraCotHalfFovY();
        float h = (float)ScreenH();
        if (cot <= 0.f || h <= 0.f) return 99999.f;
        return (h * 0.5f) * tanf(deg * 0.01745329252f) * cot;
    }

    Vec3 TargetAimPos(Il2CppObject* p, int boneMode) {
        if (!p) return Vec3();
        EnterCriticalSection(&g_targetCs);
        for (int i = 0; i < g_targetCount; ++i) {
            if (g_targets[i].player != p) continue;
            Target& t = g_targets[i];
            Vec3 h = t.headBone ? GetPos(t.headBone) : t.head;
            Vec3 f = t.footBone ? GetPos(t.footBone) : t.feet;
            LeaveCriticalSection(&g_targetCs);
            if (boneMode == 0) return h;
            if (boneMode == 2) return Vec3(h.x, (h.y + f.y) * 0.5f, h.z);
            return Vec3(h.x, h.y - (h.y - f.y) * 0.25f, h.z);
        }
        LeaveCriticalSection(&g_targetCs);
        return Vec3();
    }

    void LockTargets() { EnterCriticalSection(&g_targetCs); }
    void UnlockTargets() { LeaveCriticalSection(&g_targetCs); }

    void* MethodPtr(MethodInfo* m) {
        if (!m) return nullptr;
        void* p0 = nullptr; void* p8 = nullptr;
        se_read_ptr(m, 0, &p0);
        se_read_ptr(m, 8, &p8);
        if (p0 && se_is_exec(p0)) return p0;
        if (p8 && se_is_exec(p8)) return p8;
        return nullptr;
    }

    void DumpFields(Il2CppClass* k, const char* tag) {
        if (!k) { Log("[dump] classe %s nula", tag ? tag : "?"); return; }
        Log("[dump] ---- fields de %s ----", tag ? tag : "?");
        void* iter = nullptr;
        for (;;) {
            FieldInfo* f = se_class_get_fields(k, &iter);
            if (!f) break;
            const char* n = se_field_name(f);
            if (n) Log("[dump]   %-34s @ 0x%04X", n, (unsigned)se_field_offset(f));
        }
        Log("[dump] ---- fim ----");
    }

    void DumpFields(const char* className) { DumpFields(FindClass(className), className); }

    // ======================= weapon stats =======================
    // >>> FIX: offsets resolvidos PELA CLASSE DA INSTANCIA.
    static void EnsureGunOffsets(Il2CppClass* k) {
        if (!k || k == g_gsCls) return;
        const char* kn = se_class_name(k);
        if (!kn || (!strstr(kn, "gunStats") && !strstr(kn, "GunStats"))) return;

        g_gsCls = k;
        O_gsMagSize = PickFieldOff(k, { "magSize", "magazineSize", "maxAmmo" }, "Int32");
        O_gsAmmo = PickFieldOff(k, { "ammoInMagazine", "currentAmmo", "curAmmo" }, "Int32");
        O_gsGunObj = PickFieldOff(k, { "gunObject", "gunObj" });
        if (!O_gsMagSize) O_gsMagSize = 0xFC;
        if (!O_gsAmmo)    O_gsAmmo = 0x90;
        if (!O_gsGunObj)  O_gsGunObj = 0x148;

        Log("[sdk] gunStats offsets (cls=%s): mag=0x%X ammo=0x%X gunObj=0x%X",
            kn, O_gsMagSize, O_gsAmmo, O_gsGunObj);
    }

    static bool GunStatsLooksLive(Il2CppObject* s) {
        if (!s) return false;
        Il2CppClass* k = se_object_get_class(s);
        if (!k) return false;
        EnsureGunOffsets(k);

        if (O_gsGunObj && SafeReadObjPub(s, O_gsGunObj)) return true;
        if (O_gsMagSize && SafeRead<int>((uintptr_t)s + O_gsMagSize, 0) > 0) return true;
        if (O_gsAmmo && SafeRead<int>((uintptr_t)s + O_gsAmmo, 0) > 0) return true;
        return false;
    }

    // >>> MELHORIA: a arma equipada e a que tem o gunObject ATIVO na cena.
    static bool GunStatsEquipped(Il2CppObject* s) {
        if (!s || !O_gsGunObj) return false;
        Il2CppObject* go = SafeReadObjPub(s, O_gsGunObj);
        if (!go) return false;
        Il2CppClass* gk = se_object_get_class(go);
        if (!gk) return false;
        MethodInfo* m = se_class_get_method_from_name(gk, "get_activeInHierarchy", 0);
        if (!m) return false;
        Il2CppObject* r = se_runtime_invoke(m, go, nullptr);
        if (!r) return false;
        void* p = se_object_unbox(r);
        return p && *(unsigned char*)p != 0;
    }

    // Fonte primaria: arma NA MAO (fpsGunView). Offsets do build atual:
    //   0x30 gunStats[]  |  0x38 gunStats  |  0x48 Int32 indice (-1 = nenhuma)
    static Il2CppObject* GunStatsFromView() {
        if (!g_localPlayer || !O_gunView) return nullptr;
        Il2CppObject* v = SafeReadObjPub(g_localPlayer, O_gunView);
        if (!v) return nullptr;

        Il2CppObject* held = SafeReadObjPub(v, 0x38);
        if (GunStatsLooksLive(held)) return held;

        int idx = SafeRead<int>((uintptr_t)v + 0x48, -1);
        Il2CppObject* arr = SafeReadObjPub(v, 0x30);
        if (arr && idx >= 0) {
            int len = 0;
            if (il2cpp_array_length) len = (int)il2cpp_array_length((const Il2CppArray*)arr);
            if (len <= 0) len = SafeRead<int>((uintptr_t)arr + 0x18, 0);
            if (idx < len) {
                void* e = nullptr;
                if (se_read_ptr(arr, 0x20 + (size_t)idx * 8, &e) && e &&
                    GunStatsLooksLive((Il2CppObject*)e))
                    return (Il2CppObject*)e;
            }
        }
        return nullptr;
    }

    Il2CppObject* FindGunStatsObj() {
        if (!g_localPlayer) return nullptr;

        if (Il2CppObject* g = GunStatsFromView()) return g;

        Il2CppObject* cand = O_gunStats ? SafeReadObjPub(g_localPlayer, O_gunStats) : nullptr;
        if (GunStatsLooksLive(cand)) return cand;

        if (O_gunStatsArray) {
            Il2CppObject* arr = SafeReadObjPub(g_localPlayer, O_gunStatsArray);
            if (arr) {
                // >>> FIX: o TAMANHO fica em 0x18 (max_length). 0x10 e o ponteiro
                //     'bounds', que num array 1D e NULL -> len=0 -> nunca varria.
                int len = 0;
                if (il2cpp_array_length) len = (int)il2cpp_array_length((const Il2CppArray*)arr);
                if (len <= 0) len = SafeRead<int>((uintptr_t)arr + 0x18, 0);

                if (len > 0 && len <= 128) {
                    Il2CppObject* firstLive = nullptr;
                    int firstLiveIdx = -1;

                    for (int i = 0; i < len; ++i) {
                        void* e = nullptr;
                        if (!se_read_ptr(arr, 0x20 + (size_t)i * 8, &e) || !e) continue;
                        Il2CppObject* g = (Il2CppObject*)e;
                        if (!GunStatsLooksLive(g)) continue;

                        if (!firstLive) { firstLive = g; firstLiveIdx = i; }
                        if (!GunStatsEquipped(g)) continue;

                        static int lastEq = -1;
                        if (lastEq != i) {
                            lastEq = i;
                            Log("[sdk] gunStats EQUIPADO = loadout[%d] (%p)", i, g);
                        }
                        return g;
                    }
                    if (firstLive) {
                        static int lastIdx = -1;
                        if (lastIdx != firstLiveIdx) {
                            lastIdx = firstLiveIdx;
                            Log("[sdk] gunStats vivo = loadout[%d] (%p) [nenhum ativo]",
                                firstLiveIdx, firstLive);
                        }
                        return firstLive;
                    }
                }
            }
        }

        if (cand) return cand;

        Il2CppClass* k = se_object_get_class(g_localPlayer);
        if (k) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = se_class_get_fields(k, &it);
                if (!f) break;
                const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (!tn) continue;
                if (!strstr(tn, "Gun") && !strstr(tn, "Weapon")) continue;
                uint32_t off = (uint32_t)se_field_offset(f);
                if (!off) continue;
                Il2CppObject* s = SafeReadObjPub(g_localPlayer, off);
                if (GunStatsLooksLive(s)) return s;
            }
        }
        return nullptr;
    }

    Il2CppObject* GetGunStatsLive() { return FindGunStatsObj(); }

    int ResolveWeaponFields(Il2CppObject* stats,
        uint32_t& cur, uint32_t& max, uint32_t& reserve,
        uint32_t& reload, uint32_t& spread, uint32_t& rate, uint32_t& dmg) {
        cur = max = reserve = reload = spread = rate = dmg = 0;
        g_rateIsRpm = false;
        if (!stats) return 0;
        Il2CppClass* k = se_object_get_class(stats);
        if (!k) return 0;

        cur = PickFieldOff(k, { "ammoInMagazine", "currentAmmo", "curAmmo",
                                "mCurrentAmmo", "magazineAmmo" }, "Int32");
        max = PickFieldOff(k, { "magSize", "maxAmmo", "ammoMax",
                                "mMaxAmmo", "magazineSize", "clipSize" }, "Int32");
        reserve = PickFieldOff(k, { "totalAmmo", "reserveAmmo", "ammoReserve",
                                    "spareAmmo", "totalMags" }, "Int32");

        // >>> FIX: nao existe campo "reloading" nem "damage" neste jogo.
        reload = PickFieldOff(k, { "reloading", "isReloading", "mReloading",
                                   "hasShellReloading", "requiresClearChamber" }, "Boolean");
        if (!reload) reload = PickFieldOff(k, { "reloadTimer", "reloadingTimer",
                                                "reloadAnimationTime" }, "Single");

        spread = PickFieldOff(k, { "hipMinSpread", "spread", "currentSpread", "mSpread" }, "Single");
        if (!spread) spread = PickFieldOff(k, { "adsMinSpread" }, "Single");

        rate = PickFieldOff(k, { "shotDelay", "fireDelay", "chamberTime", "cooldown" }, "Single");
        if (!rate) {
            rate = PickFieldOff(k, { "RPM" }, "Single");
            g_rateIsRpm = (rate != 0);
        }

        dmg = PickFieldOff(k, { "damage", "damagePerShot", "bulletDamage",
                                "hitDamage", "mDamage", "dmg" });

        int n = 0;
        if (cur) ++n; if (max) ++n; if (reserve) ++n; if (reload) ++n;
        if (spread) ++n; if (rate) ++n; if (dmg) ++n;

        Log("[weapons] stats cls=%s cur=0x%X max=0x%X reserve=0x%X reload=0x%X spread=0x%X rate=0x%X(%s) dmg=0x%X",
            se_class_name(k), cur, max, reserve, reload, spread, rate,
            g_rateIsRpm ? "RPM" : "delay", dmg);
        return n;
    }

    void DumpFieldsWithValues(Il2CppObject* o) {
        if (!o) { Log("[dumpv] obj nulo"); return; }
        Il2CppClass* k = se_object_get_class(o);
        if (!k) return;
        Log("[dumpv] ---- %s @ %p ----", se_class_name(k), o);

        void* it = nullptr;
        int n = 0;
        for (;;) {
            FieldInfo* f = se_class_get_fields(k, &it);
            if (!f) break;
            const char* fn = se_field_name(f);
            if (!fn) continue;
            const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : "?";
            uint32_t off = (uint32_t)se_field_offset(f);
            if (!off) continue;

            if (tn && (strstr(tn, "Int32") || strstr(tn, "UInt32") || strstr(tn, "Int16") || strstr(tn, "Byte"))) {
                int v = 0; se_read_int(o, off, &v);
                Log("[dumpv]   %-28s : %-14s @0x%X = %d", fn, tn, off, v);
            }
            else if (tn && strstr(tn, "Single")) {
                Log("[dumpv]   %-28s : %-14s @0x%X = %.3f", fn, tn, off, se_read_float(o, off));
            }
            else if (tn && strstr(tn, "Boolean")) {
                Log("[dumpv]   %-28s : %-14s @0x%X = %d", fn, tn, off, (int)se_read_bool(o, off));
            }
            else {
                void* p = nullptr;
                if (se_read_ptr(o, off, &p))
                    Log("[dumpv]   %-28s : %-14s @0x%X = %p", fn, tn, off, p);
                else
                    Log("[dumpv]   %-28s : %-14s @0x%X = -", fn, tn, off);
            }
            if (++n > 120) break;
        }
        Log("[dumpv] ---- fim ----");
    }

    // ======================= local player =======================
    static Il2CppObject* FindLocalPlayer() {
        static const char* classes[] = { "PlayerScript", "Entity", "GameManager", "Game", "HUD" };
        static const char* fields[] = { "localPlayer", "LocalPlayer", "localEntity", "myPlayer",
                                        "myEntity", "local", "player" };
        for (size_t c = 0; c < sizeof(classes) / sizeof(classes[0]); ++c) {
            Il2CppClass* k = FindClass(classes[c]); if (!k) continue;
            for (size_t f = 0; f < sizeof(fields) / sizeof(fields[0]); ++f) {
                FieldInfo* fi = se_class_get_field_from_name(k, fields[f]); if (!fi) continue;
                Il2CppObject* v = nullptr;
                se_field_static_get_value(fi, &v);
                if (v) { Log("[sdk] localPlayer = %s.%s", classes[c], fields[f]); return v; }
            }
            MethodInfo* m = se_class_get_method_from_name(k, "get_localPlayer", 0);
            if (!m) m = se_class_get_method_from_name(k, "get_LocalPlayer", 0);
            if (m) {
                Il2CppObject* v = se_runtime_invoke(m, nullptr, nullptr);
                if (v) { Log("[sdk] localPlayer = %s::get_localPlayer", classes[c]); return v; }
            }
        }
        return nullptr;
    }

    Il2CppObject* GetLocalPlayer() {
        if (!g_localPlayer) { g_localPlayer = FindLocalPlayer(); g_local = g_localPlayer; }
        return g_localPlayer;
    }

    // ======================= alvos =======================
    static void SyncAliases(Target& t) {
        t.obj = t.player;
        t.hp = t.health;
        t.enemy = (O_team == 0) ? true : (g_localTeam < 0 || t.team < 0 || t.team != g_localTeam);
        t.lastSeen = GetTickCount64();
    }

    static void ResolveHeadFeet(Target& t) {
        const float H = 1.75f;

        bool haveHead = (t.headBone != nullptr);
        bool haveFeet = (t.footBone != nullptr);

        if (haveHead) t.head = GetPos(t.headBone);
        if (haveFeet) t.feet = GetPos(t.footBone);

        if (haveHead && haveFeet) {
            float dy = t.head.y - t.feet.y;
            if (dy < 0.6f || dy > 2.6f)
                t.feet = Vec3(t.head.x, t.head.y - H, t.head.z);
        }
        else if (haveHead) {
            t.feet = Vec3(t.head.x, t.head.y - H, t.head.z);
        }
        else if (haveFeet) {
            t.head = Vec3(t.feet.x, t.feet.y + H, t.feet.z);
        }
        else {
            t.head = t.pos;
            t.feet = Vec3(t.pos.x, t.pos.y - H, t.pos.z);
        }

        if (t.head.y < t.feet.y) {
            float mid = (t.head.y + t.feet.y) * 0.5f;
            t.head.y = mid + H * 0.5f;
            t.feet.y = mid - H * 0.5f;
        }
    }

    static bool IsLocalEntity(Il2CppObject* e) {
        if (!e) return true;
        if (e == g_localPlayer || e == g_local) return true;
        if (O_isLocal && se_read_bool(e, O_isLocal)) return true;
        if (O_localPlayerScript && kPlayerScript && IsA(e, kPlayerScript)) {
            Il2CppObject* lp = SafeReadObjPub(e, O_localPlayerScript);
            if (lp == e) return true;
        }
        return false;
    }

    static void AdoptLocal(Il2CppObject* e) {
        if (!e) return;
        const bool mudou = (e != g_localPlayer);
        g_localPlayer = e;
        g_local = e;
        g_localTeam = ReadTeam(e);
        if (mudou)
            Log("[sdk] localPlayer adotado (%p) team=%d", e, g_localTeam);
    }

    struct FoundEnt { int slot; Il2CppObject* e; };

    void RefreshTargets() {
        if (!g_localPlayer) { g_localPlayer = FindLocalPlayer(); g_local = g_localPlayer; }
        if (g_localPlayer) g_localTeam = ReadTeam(g_localPlayer);

        Il2CppClass* oc = FindClass("Object", "UnityEngine");
        if (!oc) return;
        MethodInfo* fot = se_class_get_method_from_name(oc, "FindObjectsOfType", 1);
        if (!fot) return;

        Il2CppClass* scans[2] = {
            kPlayerScript ? kPlayerScript : FindClass("PlayerScript"),
            kEntity ? kEntity : FindClass("Entity")
        };

        ++g_slotGen;

        std::vector<FoundEnt> found;
        found.reserve(MAX_TARGETS);
        int totalObjs = 0;

        for (int s = 0; s < 2; ++s) {
            Il2CppClass* sc = scans[s];
            if (!sc) continue;
            if (s == 1 && scans[0] == scans[1]) break;

            Il2CppType* t = se_class_get_type(sc);
            if (!t) continue;
            Il2CppObject* typeObj = se_type_get_object(t);
            if (!typeObj) continue;

            void* args[1]; args[0] = typeObj;
            Il2CppArray* arr = (Il2CppArray*)se_runtime_invoke(fot, nullptr, args);
            if (!arr) continue;

            uint32_t n = il2cpp_array_length ? il2cpp_array_length(arr) : 0;
            totalObjs += (int)n;

            for (uint32_t i = 0; i < n; ++i) {
                Il2CppObject* e = (Il2CppObject*)se_array_at(arr, i);
                if (!e) continue;
                if (IsLocalEntity(e)) { AdoptLocal(e); continue; }

                bool dup = false;
                for (auto& f : found) if (f.e == e) { dup = true; break; }
                if (!dup) found.push_back({ -1, e });
            }
        }

        // >>> MELHORIA: slots estaveis.
        for (auto& f : found)
            for (int i = 0; i < MAX_TARGETS; ++i)
                if (g_slotOwner[i] == f.e) { f.slot = i; g_slotSeen[i] = g_slotGen; break; }

        for (auto& f : found) {
            if (f.slot >= 0) continue;
            for (int i = 0; i < MAX_TARGETS; ++i) {
                if (!g_slotOwner[i] || g_slotSeen[i] != g_slotGen) {
                    g_slotOwner[i] = f.e; g_slotSeen[i] = g_slotGen; f.slot = i; break;
                }
            }
        }
        for (int i = 0; i < MAX_TARGETS; ++i)
            if (g_slotSeen[i] != g_slotGen) g_slotOwner[i] = nullptr;

        std::sort(found.begin(), found.end(),
            [](const FoundEnt& a, const FoundEnt& b) { return a.slot < b.slot; });

        EnterCriticalSection(&g_targetCs);
        int c = 0;
        for (auto& f : found) {
            if (c >= kMaxTargets || f.slot < 0) break;

            Target& tgt = g_targets[c];
            if (tgt.player != f.e) {
                memset(&tgt, 0, sizeof(Target));
                tgt.player = f.e;
                tgt.obj = f.e;
                tgt.slot = (uint32_t)f.slot;
                tgt.team = ReadTeam(f.e);
                tgt.pos = GetPos(f.e);
                GetNameBuf(f.e, tgt.name, sizeof(tgt.name));
                if (O_weapon) se_read_int(f.e, O_weapon, &tgt.weapon);

                Il2CppClass* ek = se_object_get_class(f.e);
                const char* en = ek ? se_class_name(ek) : nullptr;
                tgt.npc = (en && strstr(en, "NPC") != nullptr);

                ResolveBones(tgt, f.e);
            }

            tgt.health = ReadHealth(f.e);
            tgt.alive = (tgt.health > 0.5f);
            ResolveHeadFeet(tgt);
            tgt.onScreen = false;
            SyncAliases(tgt);
            ++c;
        }
        g_targetCount = c;
        LeaveCriticalSection(&g_targetCs);

        static int lastLog = -1;
        if (g_targetCount != lastLog) {
            lastLog = g_targetCount;
            Log("[sdk] scan: objetos=%d alvos=%d local=%p team=%d",
                totalObjs, g_targetCount, g_localPlayer, g_localTeam);
        }

        static bool dumped = false;
        if (!dumped && g_targetCount > 0) {
            dumped = true;
            Target& t = g_targets[0];
            Log("[diag] alvo=%p pos=(%.2f,%.2f,%.2f) head=(%.2f,%.2f,%.2f) feet=(%.2f,%.2f,%.2f) hb=%p fb=%p hp=%.1f team=%d nome=\"%s\"",
                t.player, t.pos.x, t.pos.y, t.pos.z, t.head.x, t.head.y, t.head.z,
                t.feet.x, t.feet.y, t.feet.z, t.headBone, t.footBone, t.health, t.team, t.name);
            DumpStringFields(t.player);
        }
    }

    void UpdateTargets() {
        static ULONGLONG lastRefresh = 0;
        static ULONGLONG lastTfClear = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastRefresh > 200) { lastRefresh = now; RefreshTargets(); }
        if (now - lastTfClear > 2000) { lastTfClear = now; ClearTfCache(); }

        if (!g_localPlayer) return;

        ++g_frameId;                       // >>> FIX: cache de TargetVisible
        g_localPos = GetPos(g_localPlayer);
        g_localTeam = ReadTeam(g_localPlayer);

        Il2CppObject* camTf = CamTfObj();
        Vec3 eye = camTf ? GetPos(camTf) : g_localPos;
        if (eye.zero()) eye = Vec3(g_localPos.x, g_localPos.y + 1.6f, g_localPos.z);

        EnterCriticalSection(&g_targetCs);
        for (int i = 0; i < g_targetCount; ++i) {
            Target& t = g_targets[i];
            if (!t.player) continue;

            t.health = ReadHealth(t.player);
            t.alive = (t.health > 0.5f);
            t.pos = GetPos(t.player);
            ResolveHeadFeet(t);

            float dx = t.head.x - g_localPos.x, dy = t.head.y - g_localPos.y, dz = t.head.z - g_localPos.z;
            t.dist = sqrtf(dx * dx + dy * dy + dz * dz);

            bool okH = WorldToScreen(t.head, t.scrHead);
            bool okF = WorldToScreen(t.feet, t.scrFeet);
            float h = t.scrFeet.y - t.scrHead.y;
            t.onScreen = okH && okF && (h > 1.f)
                && t.scrFeet.y > -64.f
                && t.scrHead.y < (float)ScreenH() + 64.f;

            SyncAliases(t);
        }
        LeaveCriticalSection(&g_targetCs);

        // >>> MELHORIA: LOS em round-robin (3 alvos por frame), FORA do critical
        //     section, com as DUAS referencias de transform.
        Il2CppObject* localTf = g_localPlayer ? CachedTransform(g_localPlayer) : nullptr;

        static int losCursor = 0;
        int n = g_targetCount;
        if (n > 0) {
            if (losCursor >= n) losCursor = 0;
            for (int k = 0; k < 3 && k < n; ++k) {
                int i = losCursor + k;
                if (i >= n) i -= n;
                if (!g_targets[i].player) continue;
                Il2CppObject* ttf = CachedTransform(g_targets[i].player);
                g_targets[i].visible = HasLineOfSightEx(eye, g_targets[i].head, localTf, ttf);
            }
            losCursor = (losCursor + 3) % n;
        }
    }

    std::vector<Target> Snapshot() {
        std::vector<Target> out;
        EnterCriticalSection(&g_targetCs);
        out.assign(g_targets, g_targets + g_targetCount);
        LeaveCriticalSection(&g_targetCs);
        return out;
    }

    // ======================= aim =======================
    bool AimLock(Il2CppObject* target, Quat& savedOriginal) {
        Il2CppObject* camTf = CamTfObj();
        if (!target || !camTf) return false;

        if (!GetRot(camTf, savedOriginal)) return false;
        g_savedPlayerRot = GetPlayerRot(g_localPlayer);

        Vec3 me = GetPos(camTf);
        Vec3 tp = TargetAimPos(target, C.aimBone);
        if (tp.zero()) tp = GetPos(target);
        Vec3 dir{ tp.x - me.x, tp.y - me.y, tp.z - me.z };
        float L = dir.len();
        if (L < 0.0001f) return false;
        dir = dir * (1.f / L);

        Quat q = LookRot(dir);
        if (!SetRot(camTf, q)) return false;
        if (g_localPlayer) SetPlayerRot(g_localPlayer, q);
        return true;
    }

    void RestoreAim(Il2CppObject*, const Quat& q) {
        Il2CppObject* camTf = CamTfObj();
        if (camTf) SetRot(camTf, q);
        if (g_localPlayer) SetPlayerRot(g_localPlayer, g_savedPlayerRot);
    }

    bool FireBulletReady() {
        if (!M_FireBullet) return false;
        uint32_t np = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(M_FireBullet) : 0;
        if (np == 0) return false;
        for (uint32_t i = 0; i < np; ++i) {
            const Il2CppType* t = il2cpp_method_get_param ? il2cpp_method_get_param(M_FireBullet, i) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (!tn) return false;
            if (strstr(tn, "Vector3") || strstr(tn, "Single") || strstr(tn, "Int32") ||
                strstr(tn, "Boolean") || strstr(tn, "Double") || strstr(tn, "Int64")) continue;
            return false;
        }
        return true;
    }

    void FireBulletAt(Vec3 pos) {
        if (!M_FireBullet || !g_localPlayer) return;

        Vec3 me = GetPos(g_localPlayer);
        Vec3 dir{ pos.x - me.x, pos.y - me.y, pos.z - me.z };
        float L = dir.len();
        if (L > 0.0001f) dir = dir * (1.f / L);

        const int dirSlot = g_fireDirSlot.load();
        const Vec3 muzzle = MuzzlePos();

        uint32_t np = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(M_FireBullet) : 0;
        if (np == 0) np = 1;

        // >>> FIX: a direcao e o ULTIMO Vector3 da assinatura, nao um indice
        //     fixo. O [fire7] confirma: a3 = posicao (|a3|=46.7),
        //     a4 = direcao unitaria (|a4|=1.00).
        int vCount = 0;
        for (uint32_t i = 0; i < np; ++i) {
            const Il2CppType* t = il2cpp_method_get_param ? il2cpp_method_get_param(M_FireBullet, i) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (tn && strstr(tn, "Vector3")) ++vCount;
        }
        const int dirVIndex = (dirSlot >= 0) ? dirSlot : (vCount > 0 ? vCount - 1 : 0);

        std::vector<Vec3>   vbuf(np);
        std::vector<float>  fbuf(np, 0.f);
        std::vector<int>    ibuf(np, 0);
        std::vector<double> dbuf(np, 0.0);
        std::vector<unsigned char> bbuf(np, 0);
        std::vector<void*>  args(np, nullptr);

        int vIndex = 0;
        for (uint32_t i = 0; i < np; ++i) {
            const Il2CppType* t = il2cpp_method_get_param ? il2cpp_method_get_param(M_FireBullet, i) : nullptr;
            const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            if (tn && strstr(tn, "Vector3")) {
                bool isDir = (vIndex == dirVIndex);
                vbuf[i] = isDir ? dir : muzzle;
                args[i] = &vbuf[i];
                ++vIndex;
            }
            else if (tn && strstr(tn, "Single"))  args[i] = &fbuf[i];
            else if (tn && strstr(tn, "Int32"))   args[i] = &ibuf[i];
            else if (tn && strstr(tn, "Double"))  args[i] = &dbuf[i];
            else if (tn && strstr(tn, "Boolean")) args[i] = &bbuf[i];
            else args[i] = nullptr;
        }
        se_runtime_invoke(M_FireBullet, g_localPlayer, args.data());
    }

    // ======================= init / shutdown =======================
    void Shutdown() {
        g_phase = "sdk.shutdown";
        g_localPlayer = nullptr;
        g_local = nullptr;
        g_camera = nullptr;
        g_aimTarget = nullptr;
        g_targetCount = 0;
        memset(g_slotOwner, 0, sizeof(g_slotOwner));
        memset(g_boneCache, 0, sizeof(g_boneCache));
        ClearTfCache();
    }

    static void ResolveOffsets() {
        if (kEntity) {
            O_health = PickFieldOff(kEntity, { "_health", "health", "hp" });
            O_alive = PickFieldOff(kEntity, { "shouldBeAlive", "alive", "isAlive" });
            O_isLocal = PickFieldOff(kEntity, { "isMine", "isLocalPlayer", "isOwner" }, "Boolean");
            O_rotation = PickFieldOff(kEntity, { "rotation" });
            O_camTf = PickFieldOff(kEntity, { "cameraTransform", "camTransform" }, "Transform");
            if (!O_camTf) O_camTf = PickFieldOff(kEntity, { "cameraTransform", "camTransform" });
            O_vel = PickFieldOff(kEntity, { "movingVelocity", "velocity" });
            O_name = PickFieldOff(kEntity, { "username", "playerName", "name" });
        }
        if (kPlayerScript) {
            if (!O_health) O_health = PickFieldOff(kPlayerScript, { "_health", "health", "hp" });
            if (!O_alive)  O_alive = PickFieldOff(kPlayerScript, { "shouldBeAlive", "alive", "isAlive" });
            if (!O_isLocal) O_isLocal = PickFieldOff(kPlayerScript, { "isMine", "isLocalPlayer" }, "Boolean");
            if (!O_rotation) O_rotation = PickFieldOff(kPlayerScript, { "rotation" });
            if (!O_camTf) O_camTf = PickFieldOff(kPlayerScript, { "cameraTransform", "camTransform", "mCameraTransform" }, "Transform");
            if (!O_camTf) O_camTf = PickFieldOff(kPlayerScript, { "cameraTransform", "camTransform", "mCameraTransform" });
            if (!O_name) O_name = PickFieldOff(kPlayerScript, { "username", "playerName", "name" });

            O_localPlayerScript = PickFieldOff(kPlayerScript, { "localPlayerScript" });
            O_gunStats = PickFieldOff(kPlayerScript, { "mpCurrentGunStats", "gunStats" });
            O_gunView = PickFieldOff(kPlayerScript, { "fpsGunView", "gunView" });
            O_gunStatsOnView = PickFieldOff(kPlayerScript, { "gunStatsOnView", "statsOnView" });
            O_team = PickFieldOff(kPlayerScript, { "mpTeamNumber", "team", "teamNumber" });
            O_weapon = PickFieldOff(kPlayerScript, { "weapon", "currentWeapon" });
            O_speed = PickFieldOff(kPlayerScript, { "movementSpeedMultiplier", "lastMoveVelocity" });
            O_mCamera = PickFieldOff(kPlayerScript, { "mCamera", "camera" });
            O_humanMeshAnimator = PickFieldOff(kPlayerScript, { "humanMeshAnimator", "meshAnimator", "animator" });
            O_humanMeshReferences = PickFieldOff(kPlayerScript, { "humanMeshReferences", "meshReferences" });
        }

        if (!O_rotation) O_rotation = 0x9B8;
        if (!O_camTf)    O_camTf = 0x828;
        if (!O_alive)    O_alive = 0x63C;
        if (!O_health)   O_health = 0x108;
        if (!O_gunStats) O_gunStats = 0x570;
        if (!O_team)     O_team = 0x6D0;
        if (!O_name)     O_name = 0xA68;
        if (!O_weapon)   O_weapon = 0x1A8;
        if (!O_mCamera)  O_mCamera = 0xAC8;
        if (!O_humanMeshAnimator) O_humanMeshAnimator = 0xBC0;
        if (!O_humanMeshReferences) O_humanMeshReferences = 0xA40;

        O_rot = O_rotation;
        O_cam = O_camTf;

        {
            const char* rtP = FieldTypeOf(kPlayerScript, "rotation");
            const char* rtE = FieldTypeOf(kEntity, "rotation");
            g_rotIsSyncVar = (rtP && strstr(rtP, "SyncVar")) || (rtE && strstr(rtE, "SyncVar"));

            const char* rotT = (rtP && rtP[0] != '?') ? rtP : rtE;
            O_rotationIsQuat = (!rotT) || rotT[0] == '?' || (strstr(rotT, "Quaternion") != nullptr);

            const char* hpT = FieldTypeOf(kPlayerScript, "_health");
            if (!hpT || hpT[0] == '?') hpT = FieldTypeOf(kEntity, "_health");
            g_healthIsSync = hpT && strstr(hpT, "SyncVar") != nullptr;

            const char* tmT = FieldTypeOf(kPlayerScript, "mpTeamNumber");
            if (!tmT || tmT[0] == '?') tmT = FieldTypeOf(kEntity, "team");
            g_teamIsSync = tmT && strstr(tmT, "SyncVar") != nullptr;
        }

        // classe real e a aninhada PlayerScript.gunStats.
        if (!kGunStats) kGunStats = FindClass("gunStats");
        if (!kGunStats) kGunStats = FindClass("GunStats");
        if (kGunStats) {
            O_gsMagSize = PickFieldOff(kGunStats, { "magSize", "magazineSize", "maxAmmo" }, "Int32");
            O_gsAmmo = PickFieldOff(kGunStats, { "ammoInMagazine", "currentAmmo" }, "Int32");
            O_gsGunObj = PickFieldOff(kGunStats, { "gunObject" });
        }
        if (!O_gsMagSize) O_gsMagSize = 0xFC;
        if (!O_gsAmmo)    O_gsAmmo = 0x90;
        if (!O_gsGunObj)  O_gsGunObj = 0x148;

        if (kPlayerScript && !O_gunStatsArray) {
            void* it = nullptr;
            for (;;) {
                FieldInfo* f = se_class_get_fields(kPlayerScript, &it);
                if (!f) break;
                const Il2CppType* t = il2cpp_field_get_type ? il2cpp_field_get_type(f) : nullptr;
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
                if (tn && strstr(tn, "gunStats[]")) {
                    O_gunStatsArray = (uint32_t)se_field_offset(f);
                    Log("[sdk] array de loadout = 0x%X (%s)", O_gunStatsArray, se_field_name(f));
                    break;
                }
            }
        }

        Log("[sdk] offsets: hp=0x%X(%s) name=0x%X(%s) team=0x%X(%s) rot=0x%X(%s) camTf=0x%X(%s) anim=0x%X syncRot=%d",
            O_health, FieldTypeOf(kPlayerScript, "_health"),
            O_name, FieldTypeOf(kPlayerScript, "username"),
            O_team, FieldTypeOf(kPlayerScript, "mpTeamNumber"),
            O_rotation, FieldTypeOf(kPlayerScript, "rotation"),
            O_camTf, FieldTypeOf(kPlayerScript, "cameraTransform"),
            O_humanMeshAnimator, (int)g_rotIsSyncVar);
        Log("[sdk] gunStats: mag=0x%X ammo=0x%X gunObj=0x%X array=0x%X",
            O_gsMagSize, O_gsAmmo, O_gsGunObj, O_gunStatsArray);
    }

    static void ResolveMethods() {
        if (kPlayerScript) {
            M_shoot = se_class_get_method_from_name(kPlayerScript, "SemiAutoShoot", 0);
            if (!M_shoot) M_shoot = se_class_get_method_from_name(kPlayerScript, "Shoot", 0);
            M_FireBullet = se_class_get_method_from_name(kPlayerScript, "FireBullet", 7);
            M_fireBullet = M_FireBullet;
        }
        if (kTransform) {
            M_set_position = se_class_get_method_from_name(kTransform, "set_position", 1);
            M_set_forward = se_class_get_method_from_name(kTransform, "set_forward", 1);
        }
        if (kCamera) {
            M_get_fov = se_class_get_method_from_name(kCamera, "get_fieldOfView", 0);
            M_set_fov = se_class_get_method_from_name(kCamera, "set_fieldOfView", 1);
        }

        if (M_FireBullet) {
            uint32_t np = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(M_FireBullet) : 0;
            Log("[sdk] FireBullet: %u params ready=%d ret=%s", np, (int)FireBulletReady(),
                MethodReturnName(M_FireBullet));
            for (uint32_t i = 0; i < np; ++i) {
                const Il2CppType* t = il2cpp_method_get_param ? il2cpp_method_get_param(M_FireBullet, i) : nullptr;
                const char* tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : "?";
                Log("[sdk]   param%d = %s", i, tn ? tn : "?");
            }
        }
        Log("[sdk] methods: shoot=%p fire=%p", M_shoot, M_FireBullet);
    }

    bool Init() {
        g_phase = "sdk.init";
        Log("[sdk] Init: ENTROU");

        if (!IL2CPP_API::Resolve()) { Log("[sdk] Init: Resolve FALHOU"); return false; }

        Il2CppDomain* dom = se_domain_get();
        if (!dom) { Log("[sdk] Init: domain NULO"); return false; }
        se_thread_attach(dom);

        InitializeCriticalSection(&g_targetCs);
        BuildCache();

        kEntity = FindClass("Entity");
        kPlayerScript = FindClass("PlayerScript");
        kPlayer = FindClass("Player");
        kNPC = FindClass("NPC");
        kTransform = FindClass("Transform", "UnityEngine");
        kCamera = FindClass("Camera", "UnityEngine");
        kObject = FindClass("Object", "UnityEngine");
        kRaycastHit = FindClass("RaycastHit", "UnityEngine");
        kGunStats = FindClass("gunStats");
        if (!kGunStats) kGunStats = FindClass("GunStats");
        kVector3 = FindClass("Vector3", "UnityEngine");
        kQuaternion = FindClass("Quaternion", "UnityEngine");

        // >>> FIX: FindClass("AntiHack") casava StoreAntiHack.
        kAntiHack = FindAntiHackClass();
        if (!kAntiHack) {
            ListClassesContaining("AntiHack");
            ListClassesContaining("Anti");
            ListClassesContaining("Hack");
        }

        memset(g_losState, 0xFF, sizeof(g_losState));

        ResolveOffsets();
        ResolveMethods();
        BuildStringOffsets();
        ResolveRaycast();

        if (kAntiHack) {
            Log("[ac] classe=%s instance=%p", se_class_name(kAntiHack), AntiHackInstance());
            DumpFields(kAntiHack, "AntiHack");
        }

        g_phase = "sdk.done";
        Log("[sdk] Init: CONCLUIDO");
        return true;
    }

} // namespace SDK
// ==== FIM DO ARQUIVO ====