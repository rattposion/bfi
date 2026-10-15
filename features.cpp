// ============================================================================
//  features.cpp — so o que e exclusivo deste arquivo:
//    Menu::Render / Menu::ApplyStyle / Aimbot::Bone / Menu_WantsMouse / Opts
//
//  NAO define: Menu::Draw (menu.cpp), ESP::Draw (esp.cpp),
//              Aimbot::Tick/Best (aimbot.cpp), Triggerbot::* (triggerbot.cpp),
//              Weapons::Tick (weapons.cpp), Combat::Tick (combat.cpp),
//              ConfigIO::* (config.cpp)
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "imgui.h"

#include "il2cpp.h"
#include "il2cpp_api.h"
#include "common.h"
#include "memory.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

// ===========================================================================
//  opcoes compartilhadas (definidas aqui, declaradas no features.h)
// ===========================================================================
namespace Opts {
    bool  espBox = true;
    bool  espName = true;
    bool  espHealth = true;
    bool  espDistance = true;
    bool  espSnapline = false;
    float espMaxDist = 400.f;
}

namespace {

    template <typename T>
    bool Chk(const char* label, T& v) {
        bool tmp = (v != 0);
        if (ImGui::Checkbox(label, &tmp)) { v = (T)(tmp ? 1 : 0); return true; }
        return false;
    }

    template <typename T>
    bool SliderI(const char* label, T& v, int lo, int hi) {
        int tmp = (int)v;
        if (ImGui::SliderInt(label, &tmp, lo, hi)) { v = (T)tmp; return true; }
        return false;
    }

    bool LookAt(Il2CppObject* who, Il2CppObject* target) {
        if (!who || !target || !SDK::O_rotation || !SDK::O_rotationIsQuat) return false;

        Il2CppClass* qc = SDK::FindClass("Quaternion", "UnityEngine");
        Il2CppClass* vc = SDK::FindClass("Vector3", "UnityEngine");
        if (!qc || !vc) return false;

        MethodInfo* lr = il2cpp_class_get_method_from_name(qc, "LookRotation", 1);
        if (!lr) return false;

        Vec3 me = SDK::GetPos(who);
        Il2CppObject* hb = SDK::FindBoneContains(target, "Head", 2048);
        Vec3 tp = hb ? SDK::GetPos(hb) : SDK::GetPos(target);
        Vec3 dir{ tp.x - me.x, tp.y - me.y, tp.z - me.z };

        Il2CppObject* bv = il2cpp_object_new(vc);
        if (!bv) return false;
        void* bp = il2cpp_object_unbox(bv);
        if (!bp) return false;
        memcpy(bp, &dir, sizeof(Vec3));

        void* args[1]; args[0] = bv;
        Il2CppObject* boxed = il2cpp_runtime_invoke(lr, nullptr, args, nullptr);
        if (!boxed) return false;
        void* p = il2cpp_object_unbox(boxed);
        if (!p) return false;

        Quat q;
        memcpy(&q, p, sizeof(Quat));
        WriteAt<Quat>((uintptr_t)who + SDK::O_rotation, q);
        return true;
    }

} // namespace

// ===========================================================================
//  Menu
// ===========================================================================
namespace Menu {

    void ApplyStyle() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 6.f;
        s.FrameRounding = 4.f;
        s.GrabRounding = 4.f;
        s.WindowBorderSize = 1.f;
        s.FramePadding = ImVec2(8, 4);
        s.ItemSpacing = ImVec2(8, 6);
        ImGui::StyleColorsDark();
        s.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.94f);
        s.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    }

    void Render() {
        if (!C.menuOpen) return;

        static bool styled = false;
        if (!styled) { styled = true; ApplyStyle(); }

        ImGui::SetNextWindowSize(ImVec2(340, 540), ImGuiCond_FirstUseEver);
        bool aberto = true;
        if (!ImGui::Begin("BFI", &aberto)) { ImGui::End(); return; }

        ImGui::Text("alvos: %d   desenhados: %d", SDK::g_targetCount, SDK::dbg_drawn);
        ImGui::Separator();

        if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
            Chk("ativo", C.esp);
            Chk("caixa", Opts::espBox);
            Chk("nome", Opts::espName);
            Chk("vida", Opts::espHealth);
            Chk("distancia", Opts::espDistance);
            Chk("linha", Opts::espSnapline);
            ImGui::SliderFloat("dist max", &Opts::espMaxDist, 50.f, 1500.f, "%.0f");
        }

        if (ImGui::CollapsingHeader("Aimbot", ImGuiTreeNodeFlags_DefaultOpen)) {
            Chk("ativo", C.aimbot);
            ImGui::SliderFloat("fov", &C.aimFov, 1.f, 180.f, "%.0f");
            ImGui::Text("tecla: 0x%02X", (int)C.aimKey);
            Chk("circulo de fov", C.showFovCircle);
            Chk("silent aim", C.silentAim);
            Chk("magic bullets", C.magicBullets);
        }

        if (ImGui::CollapsingHeader("Armas")) {
            Chk("no recoil", C.noRecoil);
            Chk("triggerbot", C.triggerbot);
            SliderI("delay ms", C.triggerDelay, 0, 300);
        }

        if (ImGui::CollapsingHeader("Overlay")) {
            Chk("watermark", C.watermark);
            Chk("crosshair", C.crosshair);
            ImGui::SliderFloat("tam crosshair", &C.crosshairSize, 1.f, 30.f, "%.0f");
            ImGui::ColorEdit4("cor crosshair", C.crosshairColor);
            Chk("hitmarker", C.hitmarker);
            ImGui::ColorEdit4("cor aim", C.aimColor);
        }

        if (ImGui::CollapsingHeader("Movement")) {
            Chk("speedhack", C.speedHack);
            ImGui::SliderFloat("multiplicador", &C.speedValue, 0.5f, 10.f, "%.1f");
            Chk("fly", C.fly);
            Chk("super jump", C.superJump);
            Chk("teleport (T)", C.teleport);
            Chk("godmode", C.godmode);
        }

        if (ImGui::CollapsingHeader("Camera")) {
            ImGui::SliderFloat("fov da camera", &C.camFov, 0.f, 140.f, "%.0f");
            ImGui::Text("0 = nao mexer");
        }

        if (ImGui::CollapsingHeader("AntiCheat")) {
            Chk("ativo", C.antiCheat);
            Chk("limpar blocked", C.blockedUnlock);
            Chk("limpar noclip", C.blockedNoZone);
            SliderI("stage", C.stage, 0, 20);
        }

        ImGui::Separator();
        ImGui::Text("local: %p  cam: %p", SDK::g_localPlayer, SDK::g_camera);
        ImGui::Text("view ok: %d", (int)SDK::g_viewProjOk);

        ImGui::End();

        if (!aberto) C.menuOpen = false;
    }

} // namespace Menu

bool Menu_WantsMouse() {
    return C.menuOpen && ImGui::GetIO().WantCaptureMouse;
}

// ===========================================================================
//  Aimbot::Bone — unico simbolo de Aimbot que vive aqui
// ===========================================================================
namespace Aimbot {

    void Bone(Il2CppObject* p, Il2CppObject* target) {
        if (p && target) LookAt(p, target);
    }

} // namespace Aimbot