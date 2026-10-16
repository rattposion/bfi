// Premium ImGui menu — drop-in replacement for menu.cpp
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "imgui.h"
#include "common.h"
#include "config.h"
#include "sdk.h"
#include "hooks.h"
#include "features.h"
#include "menu.h"
#include <unordered_map>

extern HMODULE g_hModule;
namespace Menu {
	namespace {
		std::unordered_map<ImGuiID, float> toggleAnimations;
		bool AnimatedSwitch(const char* label, bool* value) {
			ImGui::PushID(label);
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float width = 42.f, height = 23.f;
			ImGui::InvisibleButton("##switch", ImVec2(width, height));
			const bool clicked = ImGui::IsItemClicked();
			if (clicked) *value = !*value;
			ImGuiID id = ImGui::GetItemID();
			float& t = toggleAnimations[id];
			t += ((*value ? 1.f : 0.f) - t) * std::min(1.f, ImGui::GetIO().DeltaTime * 13.f);
			ImDrawList* d = ImGui::GetWindowDrawList();
			ImVec4 off(.19f, .21f, .28f, 1.f), on(.60f, .43f, 1.f, 1.f);
			ImVec4 c(off.x + (on.x - off.x) * t, off.y + (on.y - off.y) * t, off.z + (on.z - off.z) * t, 1.f);
			d->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ImGui::GetColorU32(c), height * .5f);
			d->AddCircleFilled(ImVec2(p.x + 11.f + 20.f * t, p.y + height * .5f), 8.f, IM_COL32(248, 248, 255, 255));
			ImGui::SameLine(0, 12.f); ImGui::TextUnformatted(label);
			ImGui::PopID(); return clicked;
		}

		int page = 0;
		int binding = -1;
		float pageFade = 1.f;
		float navGlow[7] = {};
		const char* pages[] = { "Aimbot", "Visual", "Armas", "Movimento", "Misc", "AntiCheat", "Config" };
		const ImVec4 accent(0.60f, 0.43f, 1.f, 1.f);
		float Approach(float a, float b, float speed) { return a + (b - a) * std::min(1.f, ImGui::GetIO().DeltaTime * speed); }
		const char* KeyName(int vk) {
			static char buf[32];
 switch (vk) { case 0:return "Nenhuma"; case VK_LBUTTON:return "Mouse 1"; case VK_RBUTTON:return "Mouse 2"; case VK_MBUTTON:return "Mouse 3"; case VK_SHIFT:return "Shift"; case VK_CONTROL:return "Ctrl"; case VK_MENU:return "Alt"; case VK_INSERT:return "Insert"; case VK_DELETE:return "Delete"; case VK_SPACE:return "Space"; default:break; }
					 if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { buf[0] = (char)vk;buf[1] = 0;return buf; }
					 if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buf, "F%d", vk - VK_F1 + 1);return buf; }
					 sprintf_s(buf, "0x%02X", vk);return buf;
		}
		void KeyBind(const char* label, int* value, int id) {
			ImGui::PushID(id);
			ImGui::TextUnformatted(label); ImGui::SameLine();
			const bool active = binding == id;
			if (ImGui::Button(active ? "Pressione uma tecla..." : KeyName(*value), ImVec2(145, 27))) binding = id;
			if (active) {
				if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { binding = -1; }
				else for (int k = 1;k < 254;k++) if (k != VK_LBUTTON && (GetAsyncKeyState(k) & 1)) { *value = k;binding = -1;break; }
			}
			ImGui::PopID();
		}
		void Section(const char* name, const char* subtitle = nullptr) {
			ImGui::Spacing(); ImGui::TextColored(accent, "%s", name);
			if (subtitle) ImGui::TextDisabled("%s", subtitle);
			ImGui::Separator(); ImGui::Spacing();
		}
		void TwoColumnsStart() { ImGui::Columns(2, "settings_columns", false);ImGui::SetColumnWidth(0, 300.f); }
		void TwoColumnsEnd() { ImGui::Columns(1); }
		void Content() {
			switch (page) {
			case 0: {
				ImGui::Columns(3, "aim_cards", false);
				ImGui::SetColumnWidth(0, 315); ImGui::SetColumnWidth(1, 650);
				ImGui::BeginChild("##principal", ImVec2(0, 360), true);
				Section("PRINCIPAL", "Assistencia de mira");
				AnimatedSwitch("Aimbot", &C.aimbot); AnimatedSwitch("Ignorar time", &C.aimTeam);
				AnimatedSwitch("Somente visiveis", &C.aimVisible); AnimatedSwitch("Girar jogador", &C.aimRotate);
				AnimatedSwitch("Silent aim", &C.silentAim); AnimatedSwitch("Magic bullets", &C.magicBullets);
				ImGui::EndChild(); ImGui::NextColumn();
				ImGui::BeginChild("##precision", ImVec2(0, 360), true);
				Section("CONFIGURACOES", "Precisao e prioridade");
				ImGui::SliderFloat("FOV", &C.aimFov, 1.f, 180.f, "%.1f");
				ImGui::SliderFloat("Smooth", &C.aimSmooth, 1.f, 20.f, "%.1f");
				ImGui::SliderFloat("Distancia maxima", &C.aimMaxDist, 10.f, 1000.f, "%.0f m");
				ImGui::Combo("Osso", &C.aimBone, "Cabeca\0Pescoco\0Peito\0");
				ImGui::Combo("Modo", &C.aimMode, "Mais perto da mira\0Menor vida\0Mais perto\0");
				ImGui::EndChild(); ImGui::NextColumn();
				ImGui::BeginChild("##visualaim", ImVec2(0, 360), true);
				Section("TECLA E VISUAL", "Atalhos e indicadores");
				KeyBind("Tecla de mira", &C.aimKey, 0);
				AnimatedSwitch("Circulo FOV", &C.showFovCircle);
				AnimatedSwitch("Visibilidade", &C.aimVisCheck);
				ImGui::ColorEdit4("Cor do FOV", C.aimColor);
				ImGui::TextDisabled("Tecla: %s", C.aimKeyDown ? "PRESSIONADA" : "SOLTA");
				ImGui::EndChild(); ImGui::Columns(1);
				ImGui::Spacing();
				ImGui::BeginChild("##advanced", ImVec2(0, 135), true);
				Section("CONFIGURACOES AVANCADAS", "Valores atuais e estado dos controles");
				ImGui::Text("FOV: %.1f     |     Smooth: %.1f     |     Distancia: %.0f m", C.aimFov, C.aimSmooth, C.aimMaxDist);
				ImGui::TextDisabled("Os controles alteram somente campos existentes em config.h");
				ImGui::EndChild(); break;
			}
			case 1:
				Section("Sobreposicao", "Elementos visuais"); AnimatedSwitch("ESP", &C.esp);
				TwoColumnsStart();
				AnimatedSwitch("Caixa", &C.espBox); AnimatedSwitch("Nome", &C.espName); AnimatedSwitch("Barra de vida", &C.espHealth);
				AnimatedSwitch("Vida numerica", &C.espHealthNum); AnimatedSwitch("Linha", &C.espSnapline); AnimatedSwitch("Distancia", &C.espDistance);
				ImGui::NextColumn();
				AnimatedSwitch("Arma", &C.espWeapon); AnimatedSwitch("Esqueleto", &C.espSkeleton); AnimatedSwitch("Preenchimento", &C.espFill);
				AnimatedSwitch("Ignorar time", &C.teamCheck); AnimatedSwitch("Ignorar NPCs", &C.espNpc);
				TwoColumnsEnd();
				Section("Visibilidade"); AnimatedSwitch("Verificar visibilidade", &C.espVisCheck);
				if (C.espVisCheck) { AnimatedSwitch("Linha ate o alvo", &C.espVisLine);ImGui::ColorEdit4("Visivel", C.visColor);ImGui::ColorEdit4("Oculto", C.noVisColor); }
				Section("Dimensoes e cores");
				ImGui::SliderFloat("Espessura", &C.espThickness, 0.5f, 6.f, "%.1f");ImGui::SliderFloat("Distancia maxima", &C.espMaxDist, 10.f, 1000.f, "%.0f m");
				ImGui::SliderFloat("Altura do corpo", &C.espBodyH, 1.f, 2.2f, "%.2f");ImGui::SliderFloat("Escala Y", &C.espScaleY, 0.5f, 2.f, "%.2f");
				ImGui::SliderFloat("Offset Y", &C.espOffY, -100.f, 100.f, "%.0f");
				ImGui::ColorEdit4("Caixa", C.espBoxColor);ImGui::ColorEdit4("Texto", C.espTextColor);ImGui::ColorEdit4("Esqueleto", C.espSkelColor);break;
			case 2:
				Section("Armas", "Ajustes de disparo");
				AnimatedSwitch("Sem recuo", &C.noRecoil);AnimatedSwitch("Sem dispersao", &C.noSpread);AnimatedSwitch("Municao infinita", &C.infAmmo);
				AnimatedSwitch("Sem recarregar", &C.noReload);AnimatedSwitch("Tiro rapido", &C.rapidFire);
				Section("Triggerbot");AnimatedSwitch("Ativar triggerbot", &C.triggerbot);ImGui::SliderFloat("Atraso (ms)", &C.triggerDelay, 0.f, 300.f, "%.0f");break;
			case 3:
				Section("Movimento", "Velocidade e deslocamento");
				AnimatedSwitch("Speed hack", &C.speedHack);ImGui::SliderFloat("Velocidade", &C.speedValue, 1.f, 20.f, "%.1f");
				AnimatedSwitch("Fly (espaco)", &C.fly);ImGui::SliderFloat("Velocidade fly", &C.flySpeed, 1.f, 30.f, "%.1f");
				AnimatedSwitch("Super pulo", &C.superJump);ImGui::SliderFloat("Forca do pulo", &C.jumpPower, 1.f, 30.f, "%.1f");
				AnimatedSwitch("Teleporte (T)", &C.teleport);break;
			case 4:
				Section("Interface", "Elementos na tela");
				AnimatedSwitch("Crosshair", &C.crosshair);ImGui::SliderFloat("Tamanho", &C.crosshairSize, 1.f, 30.f, "%.0f");
				ImGui::ColorEdit4("Cor crosshair", C.crosshairColor);AnimatedSwitch("Hitmarker", &C.hitmarker);
				AnimatedSwitch("Watermark", &C.watermark);AnimatedSwitch("FPS", &C.fps);
				Section("Camera");ImGui::SliderFloat("FOV da camera", &C.camFov, 0.f, 140.f, "%.0f");ImGui::TextDisabled("0 = manter valor original");break;
			case 5:
				Section("AntiCheat", "Controles existentes");
				AnimatedSwitch("Ativo", &C.antiCheat);AnimatedSwitch("Limpar blocked", &C.blockedUnlock);
				AnimatedSwitch("Limpar noclip", &C.blockedNoZone);ImGui::SliderInt("Stage", &C.stage, 0, 20);break;
			case 6:
				Section("Perfis", "Persistencia local");
				if (ImGui::Button("Salvar configuracao", ImVec2(190, 35)))ConfigIO::Save();
				ImGui::SameLine();if (ImGui::Button("Carregar", ImVec2(130, 35)))ConfigIO::Load();
				ImGui::TextDisabled("Arquivo: bfi.ini (ao lado da DLL)");
				Section("Atalhos");KeyBind("Abrir menu", &C.menuKey, 1);KeyBind("Panic", &C.panicKey, 2);
				KeyBind("Salvar", &C.saveKey, 3);KeyBind("Descarregar", &C.unloadKey, 4);break;
			}
		}
	}
	void Draw() {
		ImGui::GetIO().MouseDrawCursor = C.menuOpen;
		if (!C.menuOpen) return;
		static bool styled = false;
		if (!styled) {
			styled = true; ImGui::StyleColorsDark();
			ImGuiStyle& st = ImGui::GetStyle();
			st.WindowRounding = 13;st.ChildRounding = 12;st.FrameRounding = 8;st.PopupRounding = 8;
			st.GrabRounding = 8;st.ScrollbarRounding = 8;st.WindowPadding = ImVec2(12, 12);
			st.FramePadding = ImVec2(10, 8);st.ItemSpacing = ImVec2(10, 12);
			st.ChildBorderSize = 1;st.WindowBorderSize = 1;
			ImVec4* c = st.Colors;
			c[ImGuiCol_WindowBg] = ImVec4(.025f, .022f, .055f, 1);
			c[ImGuiCol_ChildBg] = ImVec4(.052f, .048f, .094f, 1);
			c[ImGuiCol_FrameBg] = ImVec4(.095f, .088f, .16f, 1);
			c[ImGuiCol_FrameBgHovered] = ImVec4(.17f, .12f, .29f, 1);
			c[ImGuiCol_FrameBgActive] = ImVec4(.25f, .14f, .42f, 1);
			c[ImGuiCol_Button] = ImVec4(.14f, .10f, .26f, 1);
			c[ImGuiCol_ButtonHovered] = ImVec4(.33f, .17f, .59f, 1);
			c[ImGuiCol_ButtonActive] = ImVec4(.43f, .20f, .77f, 1);
			c[ImGuiCol_Header] = ImVec4(.19f, .11f, .34f, 1);
			c[ImGuiCol_HeaderHovered] = ImVec4(.30f, .17f, .52f, 1);
			c[ImGuiCol_HeaderActive] = ImVec4(.40f, .19f, .68f, 1);
			c[ImGuiCol_SliderGrab] = ImVec4(.65f, .34f, 1, 1);
			c[ImGuiCol_SliderGrabActive] = ImVec4(.81f, .55f, 1, 1);
			c[ImGuiCol_CheckMark] = ImVec4(.72f, .44f, 1, 1);
			c[ImGuiCol_Border] = ImVec4(.27f, .18f, .43f, .9f);
			c[ImGuiCol_Text] = ImVec4(.93f, .91f, 1, 1);
			c[ImGuiCol_TextDisabled] = ImVec4(.57f, .55f, .69f, 1);
		}
		ImGui::SetNextWindowSize(ImVec2(1280, 740), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(1000, 600), ImVec2(1900, 1100));
		if (!ImGui::Begin("BFI | Premium", &C.menuOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End();return; }
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float sidebar = 225.f;
		ImGui::BeginChild("##sidebar", ImVec2(sidebar, avail.y), true);
		ImGui::Dummy(ImVec2(0, 10));
		ImGui::TextColored(accent, "  BF  /  PREMIUM");
		ImGui::TextDisabled("  BULLET FORCE INTERNAL");
		ImGui::Dummy(ImVec2(0, 30));
		for (int i = 0;i < 7;i++) {
			navGlow[i] = Approach(navGlow[i], page == i ? 1.f : 0.f, 10.f);
			ImGui::PushID(i);
			ImGui::SetCursorPosX(12);
			if (ImGui::Selectable(pages[i], page == i, 0, ImVec2(sidebar - 32, 51)) && page != i) { page = i;pageFade = 0; }
			const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
			if (navGlow[i] > .01f) {
				ImDrawList* d = ImGui::GetWindowDrawList();
				d->AddRectFilled(ImVec2(a.x, a.y + 5), ImVec2(a.x + 4, b.y - 5), ImGui::GetColorU32(ImVec4(.65f, .33f, 1, navGlow[i])), 3);
				d->AddRect(a, b, ImGui::GetColorU32(ImVec4(.59f, .31f, .96f, navGlow[i] * .75f)), 9);
			}
			ImGui::PopID();
		}
		ImGui::Dummy(ImVec2(0, 20));
		ImGui::Separator();
		ImGui::TextColored(ImVec4(.30f, .95f, .68f, 1), "  STATUS / ONLINE");
		ImGui::TextDisabled("  FPS: %.0f", ImGui::GetIO().Framerate);
		ImGui::TextDisabled("  Build: BFI Premium");
		ImGui::EndChild();ImGui::SameLine(0, 10);
		ImGui::BeginChild("##workspace", ImVec2(0, avail.y), false);
		ImGui::BeginChild("##topbar", ImVec2(0, 65), true);
		ImGui::TextColored(accent, "BFI  /  %s", pages[page]);
		ImGui::TextDisabled("CONTROLE TOTAL  /  PERSONALIZACAO");
		ImGui::EndChild();
		ImGui::Dummy(ImVec2(0, 8));
		ImGui::BeginChild("##hero", ImVec2(0, 113), true);
		ImDrawList* d = ImGui::GetWindowDrawList();
		ImVec2 a = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
		d->AddRectFilledMultiColor(a, ImVec2(a.x + sz.x, a.y + sz.y), IM_COL32(44, 18, 86, 255), IM_COL32(14, 10, 34, 255), IM_COL32(15, 10, 39, 255), IM_COL32(39, 15, 75, 255));
		d->AddCircle(ImVec2(a.x + sz.x - 90, a.y + 55), 36, IM_COL32(147, 70, 255, 130), 48, 2);
		d->AddCircle(ImVec2(a.x + sz.x - 90, a.y + 55), 19, IM_COL32(147, 70, 255, 130), 48, 2);
		d->AddLine(ImVec2(a.x + sz.x - 137, a.y + 55), ImVec2(a.x + sz.x - 43, a.y + 55), IM_COL32(147, 70, 255, 130), 1);
		d->AddLine(ImVec2(a.x + sz.x - 90, a.y + 9), ImVec2(a.x + sz.x - 90, a.y + 101), IM_COL32(147, 70, 255, 130), 1);
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextColored(ImVec4(.85f, .73f, 1, 1), "   %s", pages[page]);
		ImGui::TextDisabled("   Ajustes e preferencias  |  Interface BFI Premium");
		ImGui::EndChild();
		ImGui::Dummy(ImVec2(0, 8));
		ImGui::BeginChild("##page", ImVec2(0, -72), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
		pageFade = Approach(pageFade, 1.f, 8.f);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * std::max(.10f, pageFade));
		Content();
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::BeginChild("##bottom", ImVec2(0, 0), true);
		if (ImGui::Button("SALVAR", ImVec2(115, 36))) ConfigIO::Save();
		ImGui::SameLine();if (ImGui::Button("CARREGAR", ImVec2(115, 36))) ConfigIO::Load();
		ImGui::SameLine();ImGui::TextDisabled("   Alvos: %d  |  Desenhados: %d  |  View: %s  |  FPS: %.0f", SDK::g_targetCount, SDK::dbg_drawn, SDK::g_viewProjOk ? "OK" : "OFF", ImGui::GetIO().Framerate);
		ImGui::EndChild();
		ImGui::EndChild();ImGui::End();
	}
} // namespace Menu
