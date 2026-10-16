// ============================================================================
//  features.cpp — Menu::Render / Menu::ApplyStyle / Aimbot::Bone / Menu_WantsMouse
//  O menu de verdade vive em menu.cpp (Menu::Draw).
//
//  >>> CORRECOES desta versao:
//   1) E0312: dentro de "namespace Aimbot" o nome 'Quat' sem qualificar
//      resolve para Aimbot::Quat (declarado em features.h), nao para o
//      ::Quat que SDK::LookRot devolve e SDK::SetPlayerRot espera. Agora
//      tudo qualificado com ::Quat.
//   2) Menu::Render aplicava o estilo UMA vez e chamava Draw, mas o
//      hk_wglSwapBuffers chamava Menu::Draw direto -> ApplyStyle NUNCA rodava
//      e o menu ficava com o estilo default. Render e o ponto de entrada.
//   3) Menu_WantsMouse chamava ImGui::GetIO() sem checar se o contexto
//      existe (crash se consultado antes do ImGui subir).
//   4) ApplyStyle idem: guarda de contexto.
//   5) Aimbot::Bone nao tratava campo de rotacao SyncVar (usava WriteAt
//      direto no offset) e montava o LookRotation na mao quando o SDK ja
//      tinha LookRot/SetPlayerRot prontos.
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
    // >>> FIX: GetIO() sem contexto e crash.
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

    // >>> FIX: este e o ponto de entrada. O hooks.cpp tem que chamar Render(),
    //     nao Draw(), senao o estilo nunca e aplicado.
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

        // >>> FIX E0312: 'Quat' aqui e Aimbot::Quat, nao ::Quat.
        ::Quat q = SDK::LookRot(dir * (1.f / L));

        // SetPlayerRot trata o caso do campo ser SyncVar<Quaternion>
        SDK::SetPlayerRot(p, q);
    }

} // namespace Aimbot
// ==== FIM DO ARQUIVO ====