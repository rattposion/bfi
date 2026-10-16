// ============================================================================
//  features.cpp — Menu::Render / Menu::ApplyStyle / Aimbot::Bone / Menu_WantsMouse
//  O menu de verdade vive em menu.cpp (Menu::Draw).
//
//  >>> UNICO dono de Menu::Render(). O menu.cpp NAO define Render, senao:
//        LNK2005 "void __cdecl Menu::Render(void)" ja definida em features.obj
//        LNK1169
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
    if (!C.menuOpen || !Hooks::g_imguiReady.load()) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

namespace Menu {

    void ApplyStyle() {
        if (!Hooks::g_imguiReady.load()) return;
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

    // Ponto de entrada chamado pelo hk_wglSwapBuffers.
    // O menu.cpp define Draw() e chama o estilo proprio dele depois deste.
    void Render() {
        static bool styled = false;
        if (!styled) {
            styled = true;
            ApplyStyle();
        }
        Draw();                        // menu.cpp
    }

} // namespace Menu

// ===========================================================================
//  Aimbot::Bone — unico simbolo de Aimbot que vive aqui
// ===========================================================================
namespace Aimbot {

    void Bone(Il2CppObject* p, Il2CppObject* target) {
        if (!p || !target || !SDK::O_rotation || !SDK::O_rotationIsQuat) return;

        Vec3 me = SDK::GetPos(p);
        Vec3 tp = SDK::TargetAimPos(target, C.aimBone);
        if (tp.zero()) tp = SDK::GetPos(target);

        Vec3 dir{ tp.x - me.x, tp.y - me.y, tp.z - me.z };
        float L = dir.len();
        if (L < 0.0001f) return;

        ::Quat q = SDK::LookRot(dir * (1.f / L));

        // SetPlayerRot trata o caso do campo ser SyncVar<Quaternion>
        SDK::SetPlayerRot(p, q);
    }

} // namespace Aimbot