#include "features.h"

static int g_tab = 0;

static void Chk(const char* label, bool* v) { ImGui::Checkbox(label, v); }
static void Sld(const char* label, float* v, float lo, float hi, const char* fmt = "%.1f") {
    ImGui::SliderFloat(label, v, lo, hi, fmt);
}

void MenuDraw() {
    if (!C.menuOpen) return;

    ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Venice BF", &C.menuOpen)) { ImGui::End(); return; }

    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("ESP")) {
            g_tab = 0;
            Chk("Ativo", &C.esp); ImGui::SameLine(); Chk("Caixa", &C.espBox);
            Chk("Nome", &C.espName); ImGui::SameLine(); Chk("Distancia", &C.espDistance);
            Chk("Vida", &C.espHealth); ImGui::SameLine(); Chk("Vida num", &C.espHealthNum);
            Chk("Snapline", &C.espSnapline); ImGui::SameLine(); Chk("Team check", &C.teamCheck);
            ImGui::ColorEdit4("Cor", C.espColor);
            Sld("Altura corpo", &C.espBodyH, 0.8f, 2.5f, "%.2f");
            Sld("Offset Y", &C.espOffY, -300.f, 300.f, "%.0f");
            Sld("Escala Y", &C.espScaleY, 0.5f, 2.f, "%.2f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Aim")) {
            Chk("Aimbot", &C.aimbot); ImGui::SameLine(); Chk("Visivel", &C.aimVisible);
            Chk("Suavizar", &C.aimRotate); ImGui::SameLine(); Chk("Prever", &C.aimPredict);
            Chk("Silent", &C.silentAim); ImGui::SameLine(); Chk("Magic", &C.magicBullets);
            Sld("FOV", &C.aimFov, 20.f, 600.f, "%.0f");
            Sld("Smooth", &C.aimSmooth, 1.f, 20.f, "%.1f");
            Sld("Predict ms", &C.aimPredictMs, 0.f, 300.f, "%.0f");
            const char* bones[] = { "Cabeca", "Peito", "Corpo" };
            ImGui::Combo("Osso", &C.aimBone, bones, 3);
            const char* modes[] = { "FOV", "Vida", "Distancia" };
            ImGui::Combo("Modo", &C.aimMode, modes, 3);
            Chk("Circulo FOV", &C.showFovCircle);
            ImGui::Text("Tecla aim: 0x%X (config)", C.aimKey);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Combate")) {
            Chk("No recoil", &C.noRecoil); ImGui::SameLine(); Chk("No spread", &C.noSpread);
            Chk("Godmode", &C.godmode); ImGui::SameLine(); Chk("Dano x", &C.damageMult);
            Sld("Multiplicador", &C.damageValue, 1.f, 20.f, "%.1f");
            Chk("Triggerbot", &C.triggerbot);
            Sld("Delay ms", &C.triggerDelay, 1.f, 500.f, "%.0f");
            Chk("Anti-cheat bypass", &C.antiCheat);
            ImGui::Text("nozone=%d unlock=%d", C.blockedNoZone, C.blockedUnlock);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Armas/Mov")) {
            Chk("Municao infinita", &C.infAmmo); ImGui::SameLine(); Chk("Sem reload", &C.noReload);
            Chk("Rapid fire", &C.rapidFire);
            Chk("Speedhack", &C.speedHack);
            Sld("Velocidade", &C.speedValue, 1.f, 10.f, "%.2f");
            Chk("Super pulo", &C.superJump); ImGui::SameLine(); Chk("Voar", &C.fly);
            Chk("Teleporte (T)", &C.teleport);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("HUD/Config")) {
            Chk("Watermark", &C.watermark); ImGui::SameLine(); Chk("Mira", &C.crosshair);
            Chk("Radar", &C.radar); ImGui::SameLine(); Chk("Hitmarker", &C.hitmarker);
            Sld("FOV camera (0=off)", &C.camFov, 0.f, 140.f, "%.0f");
            if (ImGui::Button("Salvar config")) ConfigIO::Save();
            ImGui::SameLine();
            if (ImGui::Button("Recarregar"))   ConfigIO::Load();
            ImGui::Text("menu=0x%X panic=0x%X save=0x%X unload=0x%X",
                C.menuKey, C.panicKey, C.saveKey, C.unloadKey);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}