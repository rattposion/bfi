// ============================================================================
//  features.cpp — Menu::Render / Menu::ApplyStyle / Aimbot::Bone / Menu_WantsMouse
//  O menu de verdade vive em menu.cpp (Menu::Draw).
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

bool Menu_WantsMouse() {
    return C.menuOpen && ImGui::GetIO().WantCaptureMouse;
}

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
        static bool styled = false;
        if (!styled) { styled = true; ApplyStyle(); }
        Draw();                        // menu.cpp
    }

} // namespace Menu

// ===========================================================================
//  Aimbot::Bone — unico simbolo de Aimbot que vive aqui
// ===========================================================================
namespace Aimbot {

    void Bone(Il2CppObject* p, Il2CppObject* target) {
        if (!p || !target || !SDK::O_rotation || !SDK::O_rotationIsQuat) return;

        Il2CppClass* qc = SDK::FindClass("Quaternion", "UnityEngine");
        Il2CppClass* vc = SDK::FindClass("Vector3", "UnityEngine");
        if (!qc || !vc) return;

        MethodInfo* lr = il2cpp_class_get_method_from_name(qc, "LookRotation", 1);
        if (!lr) return;

        Vec3 me = SDK::GetPos(p);
        Vec3 tp = SDK::TargetAimPos(target, C.aimBone);
        Vec3 dir{ tp.x - me.x, tp.y - me.y, tp.z - me.z };

        Il2CppObject* bv = il2cpp_object_new(vc);
        if (!bv) return;
        void* bp = il2cpp_object_unbox(bv);
        if (!bp) return;
        memcpy(bp, &dir, sizeof(Vec3));

        void* args[1]; args[0] = bv;
        Il2CppObject* boxed = il2cpp_runtime_invoke(lr, nullptr, args, nullptr);
        if (!boxed) return;
        void* p2 = il2cpp_object_unbox(boxed);
        if (!p2) return;

        Quat q;
        memcpy(&q, p2, sizeof(Quat));
        WriteAt<Quat>((uintptr_t)p + SDK::O_rotation, q);
    }

} // namespace Aimbot