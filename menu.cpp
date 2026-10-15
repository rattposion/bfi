// ============================================================================
//  menu.cpp
//  Define void Menu::Draw(void)  -> casa com ?Draw@Menu@@YAXXZ
//  Salva/carrega direto em bfi.ini (nao depende de Config::Save)
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

    // ---------------------------------------------------------------------------
    static void IniPath(char* out, size_t n) {
        GetModuleFileNameA(g_hModule, out, (DWORD)n);
        char* s = strrchr(out, '\\');
        if (s) s[1] = 0; else out[0] = 0;
        strncat_s(out, n, "bfi.ini", _TRUNCATE);
    }

    static void SaveIni() {
        char path[MAX_PATH];
        IniPath(path, sizeof(path));

        FILE* f = nullptr;
        if (fopen_s(&f, path, "w") != 0 || !f) return;

        fprintf(f, "aimbot=%d\n", C(aimbot) ? 1 : 0);
        fprintf(f, "aimKey=%d\n", C(aimKey));
        fprintf(f, "aimTeam=%d\n", C(aimTeam) ? 1 : 0);
        fprintf(f, "aimVisible=%d\n", C(aimVisible) ? 1 : 0);
        fprintf(f, "aimFov=%.3f\n", C(aimFov));
        fprintf(f, "aimSmooth=%.3f\n", C(aimSmooth));
        fprintf(f, "noRecoil=%d\n", C(noRecoil) ? 1 : 0);
        fprintf(f, "noSpread=%d\n", C(noSpread) ? 1 : 0);
        fprintf(f, "infiniteAmmo=%d\n", C(infiniteAmmo) ? 1 : 0);
        fprintf(f, "esp=%d\n", C(esp) ? 1 : 0);
        fprintf(f, "espBox=%d\n", C(espBox) ? 1 : 0);
        fprintf(f, "espName=%d\n", C(espName) ? 1 : 0);
        fprintf(f, "espLine=%d\n", C(espLine) ? 1 : 0);
        fprintf(f, "espDist=%d\n", C(espDist) ? 1 : 0);
        fprintf(f, "espFill=%d\n", C(espFill) ? 1 : 0);
        fprintf(f, "teamCheck=%d\n", C(teamCheck) ? 1 : 0);
        fprintf(f, "espThickness=%.3f\n", C(espThickness));
        fprintf(f, "espMaxDist=%.3f\n", C(espMaxDist));
        fprintf(f, "speed=%d\n", C(speed) ? 1 : 0);
        fprintf(f, "speedValue=%.3f\n", C(speedValue));
        fprintf(f, "crosshair=%d\n", C(crosshair) ? 1 : 0);
        fprintf(f, "fps=%d\n", C(fps) ? 1 : 0);
        fprintf(f, "hitmarkerTime=%.3f\n", C(hitmarkerTime));
        fprintf(f, "lastHitTime=%.3f\n", C(lastHitTime));

        fclose(f);
        Log("[menu] config salvo");
    }

    static void LoadIni() {
        char path[MAX_PATH];
        IniPath(path, sizeof(path));

        FILE* f = nullptr;
        if (fopen_s(&f, path, "r") != 0 || !f) return;

        char line[160];
        while (fgets(line, sizeof(line), f)) {
            char  k[64] = { 0 };
            float v = 0.f;
            if (sscanf_s(line, "%63[^=]=%f", k, (unsigned)sizeof(k), &v) != 2) continue;

            if (!strcmp(k, "aimbot"))       C(aimbot) = (v != 0.f);
            else if (!strcmp(k, "aimKey"))       C(aimKey) = (int)v;
            else if (!strcmp(k, "aimTeam"))      C(aimTeam) = (v != 0.f);
            else if (!strcmp(k, "aimVisible"))   C(aimVisible) = (v != 0.f);
            else if (!strcmp(k, "aimFov"))       C(aimFov) = v;
            else if (!strcmp(k, "aimSmooth"))    C(aimSmooth) = v;
            else if (!strcmp(k, "noRecoil"))     C(noRecoil) = (v != 0.f);
            else if (!strcmp(k, "noSpread"))     C(noSpread) = (v != 0.f);
            else if (!strcmp(k, "infiniteAmmo")) C(infiniteAmmo) = (v != 0.f);
            else if (!strcmp(k, "esp"))          C(esp) = (v != 0.f);
            else if (!strcmp(k, "espBox"))       C(espBox) = (v != 0.f);
            else if (!strcmp(k, "espName"))      C(espName) = (v != 0.f);
            else if (!strcmp(k, "espLine"))      C(espLine) = (v != 0.f);
            else if (!strcmp(k, "espDist"))      C(espDist) = (v != 0.f);
            else if (!strcmp(k, "espFill"))      C(espFill) = (v != 0.f);
            else if (!strcmp(k, "teamCheck"))    C(teamCheck) = (v != 0.f);
            else if (!strcmp(k, "espThickness")) C(espThickness) = v;
            else if (!strcmp(k, "espMaxDist"))   C(espMaxDist) = v;
            else if (!strcmp(k, "speed"))        C(speed) = (v != 0.f);
            else if (!strcmp(k, "speedValue"))   C(speedValue) = v;
            else if (!strcmp(k, "crosshair"))    C(crosshair) = (v != 0.f);
            else if (!strcmp(k, "fps"))          C(fps) = (v != 0.f);
            else if (!strcmp(k, "hitmarkerTime"))C(hitmarkerTime) = v;
            else if (!strcmp(k, "lastHitTime"))  C(lastHitTime) = v;
        }

        fclose(f);
        Log("[menu] config carregado");
    }

    // ---------------------------------------------------------------------------
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
                if (k == VK_INSERT) continue;                 // tecla do menu
                if (GetAsyncKeyState(k) & 0x8000) {
                    *key = k;
                    s_binding = false;
                    break;
                }
            }
        }
    }

    // ---------------------------------------------------------------------------
    void Draw() {
        ImGui::GetIO().MouseDrawCursor = C(menuOpen);
        if (!C(menuOpen)) return;

        ImGui::SetNextWindowSize(ImVec2(540, 470), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("BF Internal", &C(menuOpen))) { ImGui::End(); return; }

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.f, 1.f), "BFI - Bullet Force Internal");
        ImGui::Text("Insert = abre/fecha");
        ImGui::Separator();

        if (ImGui::BeginTabBar("bfi_tabs")) {

            if (ImGui::BeginTabItem("Aimbot")) {
                ImGui::Checkbox("Aimbot", &C(aimbot));
                ImGui::Checkbox("Time no time", &C(aimTeam));
                ImGui::Checkbox("So visivel", &C(aimVisible));
                ImGui::Checkbox("Sem recuo", &C(noRecoil));

                ImGui::Separator();
                ImGui::SliderFloat("FOV", &C(aimFov), 1.f, 180.f, "%.0f");
                ImGui::SliderFloat("Smooth", &C(aimSmooth), 1.f, 20.f, "%.1f");

                ImGui::Separator();
                KeyBind("Tecla de mira", &C(aimKey));
                ImGui::Text("Estado: %s", C(aimKeyDown) ? "ATIVO" : "solto");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Visual")) {
                ImGui::Checkbox("ESP", &C(esp));

                ImGui::Separator();
                ImGui::Checkbox("Caixa", &C(espBox));
                ImGui::Checkbox("Nome", &C(espName));
                ImGui::Checkbox("Linha", &C(espLine));
                ImGui::Checkbox("Distancia", &C(espDist));
                ImGui::Checkbox("Preenchido", &C(espFill));
                ImGui::Checkbox("Ignorar time", &C(teamCheck));

                ImGui::Separator();
                ImGui::SliderFloat("Espessura", &C(espThickness), 0.5f, 6.f, "%.1f");
                ImGui::SliderFloat("Dist. max", &C(espMaxDist), 10.f, 500.f, "%.0f m");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Armas")) {
                ImGui::Checkbox("Sem recuo", &C(noRecoil));
                ImGui::Checkbox("Sem spread", &C(noSpread));
                ImGui::Checkbox("Municao infinita", &C(infiniteAmmo));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Movimento")) {
                ImGui::Checkbox("Speed hack", &C(speed));
                ImGui::SliderFloat("Velocidade", &C(speedValue), 1.f, 10.f, "%.1f");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Misc")) {
                ImGui::Checkbox("Mira (crosshair)", &C(crosshair));
                ImGui::Checkbox("FPS", &C(fps));

                ImGui::Separator();
                ImGui::SliderFloat("Duracao hitmarker", &C(hitmarkerTime), 0.05f, 2.f, "%.2f s");
                ImGui::SliderFloat("Duracao hit", &C(lastHitTime), 0.05f, 2.f, "%.2f s");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Config")) {
                if (ImGui::Button("Salvar", ImVec2(120, 0)))   SaveIni();
                ImGui::SameLine();
                if (ImGui::Button("Carregar", ImVec2(120, 0))) LoadIni();
                ImGui::Separator();
                ImGui::Text("Arquivo: bfi.ini (ao lado da dll)");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

} // namespace Menu