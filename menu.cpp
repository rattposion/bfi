#include "features.h"
#include "hooks.h"

static bool g_open = true;
static int  g_tab = 0;

static const ImVec4 kAccent = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
static const ImVec4 kAccentDim = ImVec4(0.00f, 0.55f, 0.85f, 0.90f);
static const ImVec4 kGreen = ImVec4(0.30f, 1.00f, 0.45f, 1.00f);
static const ImVec4 kRed = ImVec4(1.00f, 0.35f, 0.35f, 1.00f);
static const ImVec4 kMuted = ImVec4(0.52f, 0.55f, 0.62f, 1.00f);

static const char* g_bones[] = { "Cabeca", "Peito", "Pelve" };
static const char* g_aimModes[] = { "Mais perto da mira", "Menor vida", "Mais perto" };
static const char* g_dirs[] = { "Cima", "Baixo", "Esquerda", "Direita", "Frente", "Tras", "Auto" };
static const char* g_magic[] = { "Off", "Nenhum", "Cabeca", "Peito", "Pelve", "Pes", "Auto" };

static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding = 9.0f;
    s.ChildRounding = 7.0f;
    s.FrameRounding = 5.0f;
    s.PopupRounding = 7.0f;
    s.GrabRounding = 5.0f;
    s.ScrollbarRounding = 7.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(12, 12);
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(8, 7);
    s.ItemInnerSpacing = ImVec2(6, 6);
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 12.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.070f, 0.072f, 0.090f, 0.985f);
    c[ImGuiCol_ChildBg] = ImVec4(0.088f, 0.092f, 0.115f, 1.000f);
    c[ImGuiCol_PopupBg] = ImVec4(0.088f, 0.092f, 0.115f, 0.985f);
    c[ImGuiCol_Border] = ImVec4(0.200f, 0.210f, 0.260f, 0.700f);
    c[ImGuiCol_FrameBg] = ImVec4(0.130f, 0.135f, 0.170f, 1.000f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.175f, 0.185f, 0.240f, 1.000f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.200f, 0.215f, 0.280f, 1.000f);
    c[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.058f, 0.075f, 1.000f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.085f, 0.090f, 0.115f, 1.000f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.088f, 0.092f, 0.115f, 1.000f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.058f, 0.075f, 1.000f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.220f, 0.230f, 0.300f, 1.000f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.280f, 0.295f, 0.380f, 1.000f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.340f, 0.355f, 0.460f, 1.000f);
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_SliderGrab] = kAccentDim;
    c[ImGuiCol_SliderGrabActive] = kAccent;
    c[ImGuiCol_Button] = ImVec4(0.150f, 0.155f, 0.195f, 1.000f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.000f, 0.600f, 0.900f, 0.850f);
    c[ImGuiCol_ButtonActive] = kAccent;
    c[ImGuiCol_Header] = ImVec4(0.150f, 0.155f, 0.195f, 1.000f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.000f, 0.600f, 0.900f, 0.600f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.000f, 0.780f, 1.000f, 0.900f);
    c[ImGuiCol_Separator] = ImVec4(0.200f, 0.210f, 0.260f, 0.600f);
    c[ImGuiCol_Text] = ImVec4(0.920f, 0.930f, 0.960f, 1.000f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.520f, 0.580f, 1.000f);
    c[ImGuiCol_PlotLines] = kAccent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.000f, 0.600f, 0.900f, 0.400f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.000f, 0.700f, 1.000f, 0.700f);
    c[ImGuiCol_ResizeGripActive] = kAccent;
}

static void SectionHeader(const char* txt) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::Text("%s", txt);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

static bool SafeCombo(const char* label, int* idx, const char* const* items, int count) {
    if (count <= 0) return false;
    if (*idx < 0 || *idx >= count) *idx = 0;

    bool changed = false;
    if (ImGui::BeginCombo(label, items[*idx])) {
        for (int i = 0; i < count; i++) {
            bool sel = (i == *idx);
            if (ImGui::Selectable(items[i], sel)) { *idx = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static void KeyBind(const char* label, int* key) {
    ImGui::PushID(label);
    char b[64];
    sprintf_s(b, "%s: 0x%02X", label, *key);
    ImGui::TextDisabled("%s", b);
    ImGui::SameLine(170);
    if (ImGui::Button("definir", ImVec2(80, 0))) {
        for (int vk = 1; vk < 0xFE; vk++) {
            if (GetAsyncKeyState(vk) & 0x8000) { *key = vk; break; }
        }
    }
    ImGui::PopID();
}

static void NavButton(const char* label, int id) {
    bool active = (g_tab == id);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, kAccentDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
    }
    if (ImGui::Button(label, ImVec2(-1, 30))) g_tab = id;
    if (active) ImGui::PopStyleColor(3);
}

static void StatusDot(bool ok, const char* txt) {
    ImGui::PushStyleColor(ImGuiCol_Text, ok ? kGreen : kRed);
    ImGui::Text("%s", ok ? "ON" : "OFF");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", txt);
    ImGui::SameLine(140);
}

static void TabESP() {
    SectionHeader("DESENHO");
    ImGui::Checkbox("Ligado##esp", &C.esp);
    ImGui::SameLine(); ImGui::Checkbox("Caixa", &C.espBox);
    ImGui::SameLine(); ImGui::Checkbox("Snapline", &C.espSnapline);
    ImGui::Checkbox("Nome", &C.espName);
    ImGui::SameLine(); ImGui::Checkbox("Distancia", &C.espDistance);
    ImGui::Checkbox("Barra de vida", &C.espHealth);
    ImGui::SameLine(); ImGui::Checkbox("Numero da vida", &C.espHealthNum);

    SectionHeader("AJUSTES");
    ImGui::Checkbox("Team check", &C.teamCheck);
    ImGui::ColorEdit4("Cor", C.espColor, ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat("altura corpo", &C.espBodyH, 0.8f, 2.5f, "%.2f m");
    ImGui::SliderFloat("ajuste Y", &C.espOffY, -300.f, 300.f, "%.0f px");
    ImGui::SliderFloat("escala Y", &C.espScaleY, 0.5f, 2.f, "%.2fx");
    ImGui::SliderFloat("FOV camera", &C.camFov, 20.f, 140.f, "%.0f");
}

static void TabAim() {
    SectionHeader("AIMBOT");
    ImGui::Checkbox("Ligado##aim", &C.aimbot);
    ImGui::SameLine(); ImGui::Checkbox("So visivel", &C.aimVisible);
    ImGui::SameLine(); ImGui::Checkbox("Girar camera", &C.aimRotate);
    ImGui::Checkbox("Prediction", &C.aimPredict);

    SectionHeader("FOV");
    ImGui::Checkbox("Mostrar circulo na tela", &C.showFovCircle);
    ImGui::SliderFloat("FOV", &C.aimFov, 20.f, 600.f, "%.0f px");

    // preview do circulo no proprio menu
    {
        float side = 160.f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 center(p.x + side * 0.5f, p.y + side * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(p, ImVec2(p.x + side, p.y + side), IM_COL32(20, 22, 30, 255), 6.f);
        dl->AddRect(p, ImVec2(p.x + side, p.y + side), IM_COL32(80, 90, 110, 255), 6.f);

        float maxR = side * 0.5f - 4.f;
        float shownR = (C.aimFov / 600.f) * maxR;
        if (shownR < 2.f) shownR = 2.f;
        if (shownR > maxR) shownR = maxR;

        dl->AddCircle(center, shownR, IM_COL32(0, 200, 255, 220), 64, 1.5f);
        dl->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y), IM_COL32(255, 255, 255, 180));
        dl->AddLine(ImVec2(center.x, center.y - 5), ImVec2(center.x, center.y + 5), IM_COL32(255, 255, 255, 180));

        char b[32]; sprintf_s(b, "%.0f px", C.aimFov);
        dl->AddText(ImVec2(p.x + 6, p.y + 6), IM_COL32(200, 220, 255, 230), b);

        ImGui::Dummy(ImVec2(side, side));
    }

    SectionHeader("COMPORTAMENTO");
    ImGui::SliderFloat("Smooth", &C.aimSmooth, 1.f, 20.f, "%.0f");
    ImGui::SliderFloat("Deadzone", &C.aimDeadzone, 0.f, 10.f, "%.1f px");
    ImGui::SliderFloat("Predict ms", &C.aimPredictMs, 0.f, 200.f, "%.0f ms");
    SafeCombo("Osso", &C.aimBone, g_bones, 3);
    SafeCombo("Modo", &C.aimMode, g_aimModes, 3);
    KeyBind("Tecla aim", &C.aimKey);

    SectionHeader("SILENT / MAGIC");
    ImGui::Checkbox("Silent aim", &C.silentAim);
    SafeCombo("Direcao", &C.silentDirIndex, g_dirs, 7);
    ImGui::Checkbox("Magic bullets", &C.magicBullets);
    SafeCombo("Alvo magic", &C.magicIndex, g_magic, 7);
    ImGui::SliderFloat("Raio magic", &C.magicRadius, 0.2f, 10.f, "%.2f m");

    SectionHeader("TRIGGERBOT");
    ImGui::Checkbox("Ligado##trig", &C.triggerbot);
    ImGui::SliderFloat("Delay", &C.triggerDelay, 0.f, 300.f, "%.0f ms");
}

static void TabArmas() {
    SectionHeader("RECUO / SPREAD");
    ImGui::Checkbox("No recoil", &C.noRecoil);
    ImGui::SameLine(); ImGui::Checkbox("No spread", &C.noSpread);

    SectionHeader("MUNICAO");
    ImGui::Checkbox("Municao infinita", &C.infAmmo);
    ImGui::SameLine(); ImGui::Checkbox("Sem reload", &C.noReload);
    ImGui::Checkbox("Tiro rapido", &C.rapidFire);

    SectionHeader("VIDA / DANO");
    ImGui::Checkbox("Godmode", &C.godmode);
    ImGui::SameLine(); ImGui::Checkbox("Sem flinch", &C.noFlinch);
    ImGui::Checkbox("Multiplicar dano", &C.damageMult);
    ImGui::SliderFloat("Dano x", &C.damageValue, 0.5f, 20.f, "%.1fx");
}

static void TabMov() {
    SectionHeader("MOVIMENTO");
    ImGui::Checkbox("Speed hack", &C.speedHack);
    ImGui::SliderFloat("Velocidade", &C.speedValue, 1.f, 5.f, "%.2fx");
    ImGui::Checkbox("Super pulo", &C.superJump);
    ImGui::SameLine(); ImGui::Checkbox("Voar", &C.fly);
    ImGui::Checkbox("Teleport (T)", &C.teleport);
}

static void TabVisual() {
    SectionHeader("HUD");
    ImGui::Checkbox("Watermark", &C.watermark);
    ImGui::SameLine(); ImGui::Checkbox("Mira", &C.crosshair);
    ImGui::SameLine(); ImGui::Checkbox("Hitmarker", &C.hitmarker);
    ImGui::Checkbox("Radar", &C.radar);

    SectionHeader("ANTI-CHEAT");
    ImGui::Checkbox("Bloquear anti-cheat", &C.antiCheat);
    ImGui::Spacing();
    ImGui::TextDisabled("nozone bloqueado:");
    ImGui::SameLine(170);
    ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
    ImGui::Text("%d", C.blockedNoZone);
    ImGui::PopStyleColor();
    ImGui::TextDisabled("unlock forcado:");
    ImGui::SameLine(170);
    ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
    ImGui::Text("%d", C.blockedUnlock);
    ImGui::PopStyleColor();
}

static void TabDebug() {
    SectionHeader("ESTADO");
    StatusDot(Hooks::g_lateHooked != 0, "LateUpdate");
    StatusDot(SDK::dbg_ok_w2s != 0, "WorldToScreen");
    StatusDot(SDK::dbg_ok_find != 0, "FindObjects");
    StatusDot(SDK::dbg_ok_camTf != 0, "cameraTransform");

    ImGui::Spacing();
    ImGui::TextDisabled("local:");
    ImGui::SameLine(110); ImGui::Text("%p", (void*)SDK::dbg_local);
    ImGui::TextDisabled("alvos:");
    ImGui::SameLine(110); ImGui::Text("%d", g_targetCount);
    ImGui::TextDisabled("scan:");
    ImGui::SameLine(110); ImGui::Text("%d", SDK::dbg_scanCount);
    ImGui::TextDisabled("desenhados:");
    ImGui::SameLine(110); ImGui::Text("%d", SDK::dbg_drawn);
    ImGui::TextDisabled("rejeitados:");
    ImGui::SameLine(110); ImGui::Text("%d", SDK::dbg_reject);
    ImGui::TextDisabled("adds:");
    ImGui::SameLine(110); ImGui::Text("%d", SDK::dbg_adds);

    if (g_targetCount > 0) {
        SectionHeader("ALVO 0");
        ImGui::TextDisabled("nome:");
        ImGui::SameLine(110); ImGui::Text("%s", g_targets[0].name.c_str());
        ImGui::TextDisabled("vida:");
        ImGui::SameLine(110); ImGui::Text("%d%%", g_targets[0].health);
        ImGui::TextDisabled("team:");
        ImGui::SameLine(110); ImGui::Text("%d", g_targets[0].team);
        ImGui::TextDisabled("dist:");
        ImGui::SameLine(110); ImGui::Text("%.1f m", g_targets[0].dist);
        Vec2 s;
        if (SDK::WorldToScreen(g_targets[0].head, s)) {
            ImGui::TextDisabled("tela:");
            ImGui::SameLine(110); ImGui::Text("%.0f %.0f", s.x, s.y);
        }
        else {
            ImGui::TextDisabled("tela:");
            ImGui::SameLine(110); ImGui::Text("FORA/atras");
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Escanear agora", ImVec2(-1, 28))) SDK::ScanPlayers();
}

void MenuDraw() {
    static bool lastKey = false;
    bool nowKey = (GetAsyncKeyState(C.menuKey) & 0x8000) != 0;
    if (nowKey && !lastKey) {
        g_open = !g_open;
        C.menuOpen = g_open;
        if (!g_open) ConfigIO::Save();
    }
    lastKey = nowKey;

    if (!g_open) return;

    static bool styled = false;
    if (!styled) { ApplyStyle(); styled = true; }

    if (C.aimBone < 0 || C.aimBone > 2)               C.aimBone = 0;
    if (C.aimMode < 0 || C.aimMode > 2)               C.aimMode = 0;
    if (C.silentDirIndex < 0 || C.silentDirIndex > 6) C.silentDirIndex = 1;
    if (C.magicIndex < 0 || C.magicIndex > 6)         C.magicIndex = 2;

    ImGui::SetNextWindowSize(ImVec2(640, 540), ImGuiCond_FirstUseEver);
    ImGui::Begin("Venice BF | nvapi64", &g_open, ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::Text("VENICE BF");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("v1.0");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, g_targetCount > 0 ? kGreen : kMuted);
    ImGui::Text("alvos %d", g_targetCount);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Hooks::g_lateHooked ? kGreen : kRed);
    ImGui::Text(Hooks::g_lateHooked ? "hook ok" : "sem hook");
    ImGui::PopStyleColor();

    ImGui::Separator();

    ImGui::BeginChild("##nav", ImVec2(148, 0), true);
    NavButton("ESP", 0);
    NavButton("Aimbot", 1);
    NavButton("Armas / Vida", 2);
    NavButton("Movimento", 3);
    NavButton("Visual / AC", 4);
    NavButton("DEBUG", 5);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Menu: 0x%02X", C.menuKey);
    if (ImGui::Button("Salvar", ImVec2(-1, 26))) ConfigIO::Save();
    if (ImGui::Button("Carregar", ImVec2(-1, 26))) ConfigIO::Load();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##content", ImVec2(0, 0), true);
    switch (g_tab) {
    case 0: TabESP();     break;
    case 1: TabAim();     break;
    case 2: TabArmas();   break;
    case 3: TabMov();     break;
    case 4: TabVisual();  break;
    case 5: TabDebug();   break;
    }
    ImGui::EndChild();

    ImGui::End();
}