// ============================================================================
//  menu.cpp — Menu::Draw() : o menu completo. Escreve direto em C.*
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "common.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"

extern HMODULE g_hModule;

namespace Menu { void Draw(); }

namespace Menu {

    static bool s_binding = false;

    static const char* KeyName(int vk) {
        static char buf[32];
        switch (vk) {
        case 0:            return "None";
        case VK_LBUTTON:   return "Mouse1";
        case VK_RBUTTON:   return "Mouse2";
        case VK_MBUTTON:   return "Mouse3";
        case VK_XBUTTON1:  return "Mouse4";
        case VK_XBUTTON2:  return "Mouse5";
        case VK_SHIFT:     return "Shift";
        case VK_CONTROL:   return "Ctrl";
        case VK_MENU:      return "Alt";
        case VK_SPACE:     return "Space";
        case VK_TAB:       return "Tab";
        case VK_CAPITAL:   return "Caps";
        case VK_DELETE:    return "Del";
        case VK_INSERT:    return "Insert";
        case VK_HOME:      return "Home";
        case VK_END:       return "End";
        default: break;
        }
        if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = 0; return buf; }
        if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = 0; return buf; }
        if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buf, "F%d", vk - VK_F1 + 1); return buf; }
        sprintf_s(buf, "0x%02X", vk);
        return buf;
    }

    static void KeyBind(const char* label, int* key) {
        char txt[96];
        sprintf_s(txt, "%s: %s", label, s_binding ? "<aperte...>" : KeyName(*key));

        if (ImGui::Button(txt, ImVec2(150, 0))) s_binding = true;

        if (s_binding) {
            for (int k = 1; k < 0xFE; ++k) {
                if (GetAsyncKeyState(k) & 0x8000) {
                    *key = k;
                    s_binding = false;
                    break;
                }
            }
        }
    }

    void Draw() {
        ImGui::GetIO().MouseDrawCursor = C.menuOpen;
        if (!C.menuOpen) return;

        ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("BF Internal", &C.menuOpen)) { ImGui::End(); return; }

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.f, 1.f), "BFI - Bullet Force Internal");
        ImGui::Text("Insert = abre/fecha | F5 = salvar | Del = panic | F8 = descarregar");
        ImGui::Separator();

        if (ImGui::BeginTabBar("bfi_tabs")) {

            if (ImGui::BeginTabItem("Aimbot")) {
                ImGui::Checkbox("Aimbot", &C.aimbot);
                ImGui::Checkbox("Ignorar time", &C.aimTeam);
                ImGui::Checkbox("So visivel", &C.aimVisible);
                ImGui::Checkbox("Girar player (aim rotate)", &C.aimRotate);
                ImGui::Checkbox("Silent aim", &C.silentAim);
                ImGui::Checkbox("Magic bullets", &C.magicBullets);

                ImGui::Separator();
                ImGui::SliderFloat("FOV (graus)", &C.aimFov, 1.f, 90.f, "%.0f");
                ImGui::SliderFloat("Smooth", &C.aimSmooth, 1.f, 20.f, "%.1f");
                ImGui::Combo("Osso", &C.aimBone, "Cabeca\0Pescoco\0Peito\0");
                ImGui::Combo("Modo", &C.aimMode, "Mais perto da mira\0Menor vida\0Mais perto\0");
                ImGui::Checkbox("Circulo de FOV", &C.showFovCircle);

                ImGui::Separator();
                KeyBind("Tecla de mira", &C.aimKey);
                ImGui::Text("Estado: %s", C.aimKeyDown ? "ATIVO" : "solto");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Visual")) {
                ImGui::Checkbox("ESP", &C.esp);

                ImGui::Separator();
                ImGui::Checkbox("Caixa", &C.espBox);
                ImGui::Checkbox("Nome", &C.espName);
                ImGui::Checkbox("Vida (barra)", &C.espHealth);
                ImGui::Checkbox("Vida (numero)", &C.espHealthNum);
                ImGui::Checkbox("Linha", &C.espSnapline);
                ImGui::Checkbox("Distancia", &C.espDistance);
                ImGui::Checkbox("Arma", &C.espWeapon);
                ImGui::Checkbox("Esqueleto", &C.espSkeleton);
                ImGui::Checkbox("Preenchido", &C.espFill);
                ImGui::Checkbox("Ignorar time", &C.teamCheck);
                ImGui::Checkbox("Ignorar NPCs", &C.espNpc);

                ImGui::Separator();
                ImGui::SliderFloat("Espessura", &C.espThickness, 0.5f, 6.f, "%.1f");
                ImGui::SliderFloat("Dist. max", &C.espMaxDist, 10.f, 1000.f, "%.0f m");
                ImGui::SliderFloat("Altura corpo", &C.espBodyH, 1.f, 2.2f, "%.2f");
                ImGui::SliderFloat("Escala Y", &C.espScaleY, 0.5f, 2.f, "%.2f");
                ImGui::SliderFloat("Offset Y", &C.espOffY, -100.f, 100.f, "%.0f");
                ImGui::ColorEdit4("Cor da caixa", C.espBoxColor);
                ImGui::ColorEdit4("Cor do texto", C.espTextColor);
                ImGui::ColorEdit4("Cor do esqueleto", C.espSkelColor);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Armas")) {
                ImGui::Checkbox("Sem recuo", &C.noRecoil);
                ImGui::Checkbox("Sem spread", &C.noSpread);
                ImGui::Checkbox("Municao infinita", &C.infAmmo);
                ImGui::Checkbox("Sem recarregar", &C.noReload);
                ImGui::Checkbox("Tiro rapido", &C.rapidFire);
                ImGui::Separator();
                ImGui::Checkbox("Triggerbot", &C.triggerbot);
                ImGui::SliderFloat("Delay (ms)", &C.triggerDelay, 0.f, 300.f, "%.0f");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Movimento")) {
                ImGui::Checkbox("Speed hack", &C.speedHack);
                ImGui::SliderFloat("Velocidade", &C.speedValue, 1.f, 20.f, "%.1f");
                ImGui::Checkbox("Fly (espaco)", &C.fly);
                ImGui::SliderFloat("Vel. fly", &C.flySpeed, 1.f, 30.f, "%.1f");
                ImGui::Checkbox("Super pulo", &C.superJump);
                ImGui::SliderFloat("Forca pulo", &C.jumpPower, 1.f, 30.f, "%.1f");
                ImGui::Checkbox("Teleporte (T)", &C.teleport);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Misc")) {
                ImGui::Checkbox("Crosshair", &C.crosshair);
                ImGui::SliderFloat("Tam crosshair", &C.crosshairSize, 1.f, 30.f, "%.0f");
                ImGui::ColorEdit4("Cor crosshair", C.crosshairColor);
                ImGui::Checkbox("Hitmarker", &C.hitmarker);
                ImGui::Checkbox("Watermark", &C.watermark);
                ImGui::Checkbox("FPS", &C.fps);
                ImGui::Separator();
                ImGui::SliderFloat("FOV camera", &C.camFov, 0.f, 140.f, "%.0f");
                ImGui::Text("0 = nao mexer");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("AntiCheat")) {
                ImGui::Checkbox("Ativo", &C.antiCheat);
                ImGui::Checkbox("Limpar blocked", &C.blockedUnlock);
                ImGui::Checkbox("Limpar noclip", &C.blockedNoZone);
                ImGui::SliderInt("Stage", &C.stage, 0, 20);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Config")) {
                if (ImGui::Button("Salvar", ImVec2(120, 0))) ConfigIO::Save();
                ImGui::SameLine();
                if (ImGui::Button("Carregar", ImVec2(120, 0))) ConfigIO::Load();
                ImGui::Separator();
                ImGui::Text("Arquivo: bfi.ini (ao lado da dll)");
                KeyBind("Tecla do menu", &C.menuKey);
                KeyBind("Tecla panic", &C.panicKey);
                KeyBind("Tecla salvar", &C.saveKey);
                KeyBind("Tecla desacarregar", &C.unloadKey);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::Text("alvos: %d  desenhados: %d", SDK::g_targetCount, SDK::dbg_drawn);
        ImGui::Text("local: %p  cam: %p  viewOk: %d", SDK::g_localPlayer, SDK::g_camera, (int)SDK::g_viewProjOk);

        ImGui::End();
    }

} // namespace Menu