// ============================================================================
//  anticheat.cpp — limpeza de flags do AntiHack + linha `dbg:` de diagnostico.
//  Roda na thread principal, chamado pelo hk_lateUpdate.
// ============================================================================
#include "features.h"
#include "sdk.h"
#include "hooks.h"
#include "common.h"

#include <windows.h>
#include <initializer_list>
#include <vector>

void Log(const char* fmt, ...);

namespace AntiCheat {

    static Il2CppClass* g_cls = nullptr;
    static Il2CppObject* g_obj = nullptr;
    static uint32_t o_blocked = 0, o_speed = 0, o_fly = 0, o_noclip = 0, o_stage = 0;
    static int g_clears = 0;

    // ------------------------------------------------------------------
    static uint32_t FieldOff(Il2CppClass* k, const char* name) {
        if (!k) return 0;
        for (Il2CppClass* c = k; c; c = il2cpp_class_get_parent(c)) {
            FieldInfo* f = il2cpp_class_get_field_from_name(c, name);
            if (f) return (uint32_t)il2cpp_field_get_offset(f);
        }
        return 0;
    }

    static uint32_t Pick(Il2CppClass* k, std::initializer_list<const char*> names) {
        for (const char* n : names) {
            uint32_t o = FieldOff(k, n);
            if (o) return o;
        }
        return 0;
    }

    // ------------------------------------------------------------------
    bool Install() {
        g_cls = SDK::kAntiHack;
        if (!g_cls) g_cls = SDK::FindClass("AntiHack");
        if (!g_cls) g_cls = SDK::FindClass("AntiCheat");
        if (!g_cls) { Log("[ac] classe AntiHack/AntiCheat nao encontrada"); return false; }

        o_blocked = Pick(g_cls, { "blocked", "isBlocked", "mBlocked", "blockedByAntiCheat" });
        o_speed = Pick(g_cls, { "speedHack", "speedDetected", "speedFlag", "isSpeedHacking" });
        o_fly = Pick(g_cls, { "flyHack", "flyDetected", "flyFlag", "isFlyHacking" });
        o_noclip = Pick(g_cls, { "noclip", "noClipDetected", "noclipFlag", "isNoclip" });
        o_stage = Pick(g_cls, { "stage", "mStage", "level" });

        Log("[ac] Install cls=%p blocked=0x%X speed=0x%X fly=0x%X noclip=0x%X stage=0x%X",
            g_cls, o_blocked, o_speed, o_fly, o_noclip, o_stage);

        if (!o_blocked && !o_speed && !o_fly && !o_noclip && !o_stage)
            Log("[ac] nenhum campo reconhecido — rode SDK::DumpFields(SDK::kAntiHack, \"AntiHack\")");

        return true;
    }

    // ------------------------------------------------------------------
    static void FindInstance() {
        static uint64_t lastTry = 0;
        uint64_t now = GetTickCount64();
        if (now - lastTry < 1000) return;
        lastTry = now;

        static const char* cands[] = {
            "instance", "Instance", "m_Instance", "singleton", "m_Singleton", "current"
        };
        for (int i = 0; i < (int)(sizeof(cands) / sizeof(cands[0])); ++i) {
            FieldInfo* f = il2cpp_class_get_field_from_name(g_cls, cands[i]);
            if (!f) continue;
            Il2CppObject* o = nullptr;
            __try { il2cpp_field_static_get_value(f, &o); }
            __except (EXCEPTION_EXECUTE_HANDLER) { o = nullptr; }
            if (o) { g_obj = o; Log("[ac] instancia via '%s' (%p)", cands[i], o); return; }
        }
    }

    // ------------------------------------------------------------------
    //  diagnostico — mesma fonte que o ESP usa
    // ------------------------------------------------------------------
    static void Diag() {
        static ULONGLONG last = 0;
        ULONGLONG agora = GetTickCount64();
        if (agora - last < 1000) return;
        last = agora;

        std::vector<Target> snap = SDK::Snapshot();
        int alvos = (int)snap.size();
        int vivos = 0, comBone = 0, naTela = 0;

        for (size_t i = 0; i < snap.size(); ++i) {
            if (snap[i].alive)    ++vivos;
            if (snap[i].headBone) ++comBone;
            if (snap[i].onScreen) ++naTela;
        }

        Log("dbg: alvos=%d vivos=%d comBone=%d naTela=%d local=%p cam=%p fase=%s",
            alvos, vivos, comBone, naTela, SDK::g_localPlayer, SDK::g_camera, g_phase);
    }

    // ------------------------------------------------------------------
    void Tick() {
        PHASE("AntiCheat::Tick");

        Diag();                        // diagnostico roda mesmo sem instancia

        if (!C.antiCheat) return;
        if (!g_cls) return;
        if (!g_obj) { FindInstance(); if (!g_obj) return; }

        uintptr_t base = (uintptr_t)g_obj;

        if (o_blocked && C.blockedUnlock) WriteAt<bool>(base + o_blocked, false);
        if (o_speed)                      WriteAt<bool>(base + o_speed, false);
        if (o_fly)                        WriteAt<bool>(base + o_fly, false);
        if (o_noclip && C.blockedNoZone)  WriteAt<bool>(base + o_noclip, false);
        if (o_stage && C.stage > 0)       WriteAt<int>(base + o_stage, C.stage);

        g_clears++;
        if (g_clears % 600 == 0) Log("[ac] limpando flags (obj=%p)", g_obj);
    }

} // namespace AntiCheat