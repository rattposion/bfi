#include "config.h"
#include "common.h"

extern HMODULE g_hModule;

Config  g_cfg;
Config& C = g_cfg;

// ---------------------------------------------------------------------
static const char* CfgPath() {
    static char buf[MAX_PATH] = {};
    if (!buf[0]) {
        GetModuleFileNameA(g_hModule, buf, MAX_PATH);
        char* p = strrchr(buf, '\\');
        if (p) *(p + 1) = 0;
        strcat_s(buf, "bfi.ini");
    }
    return buf;
}

static void WCol(FILE* f, const char* k, const float c[4]) {
    fprintf(f, "%s=%.4f %.4f %.4f %.4f\n", k, c[0], c[1], c[2], c[3]);
}
static void RCol(const char* v, float c[4]) {
    sscanf_s(v, "%f %f %f %f", &c[0], &c[1], &c[2], &c[3]);
}

namespace ConfigIO {

    void Save() {
        FILE* f = nullptr;
        if (fopen_s(&f, CfgPath(), "w") != 0 || !f) return;

#define S_B(v) fprintf(f, #v "=%d\n", g_cfg.v ? 1 : 0)
#define S_I(v) fprintf(f, #v "=%d\n", g_cfg.v)
#define S_F(v) fprintf(f, #v "=%.4f\n", g_cfg.v)

        S_B(menuOpen); S_I(menuKey); S_I(panicKey); S_I(saveKey); S_I(unloadKey);

        S_B(esp); S_B(espBox); S_B(espFill); S_B(espName); S_B(espHealth);
        S_B(espHealthNum); S_B(espWeapon); S_B(espDistance); S_B(espSnapline);
        S_B(espSkeleton); S_B(espNpc); S_B(teamCheck); S_B(fps);
        S_F(espThickness); S_F(espMaxDist); S_F(espFontSize);
        S_F(espOffY); S_F(espScaleY); S_F(espBodyH);
        WCol(f, "espColor", g_cfg.espColor);
        WCol(f, "espBoxColor", g_cfg.espBoxColor);
        WCol(f, "espTextColor", g_cfg.espTextColor);
        WCol(f, "espSkelColor", g_cfg.espSkelColor);

        S_B(aimbot); S_I(aimKey); S_F(aimFov); S_F(aimSmooth); S_I(aimBone);
        S_I(aimMode); S_B(aimVisible); S_B(aimTeam); S_B(aimRotate);
        S_B(showFovCircle); S_F(aimMaxDist);
        WCol(f, "aimColor", g_cfg.aimColor);

        S_B(silentAim); S_B(magicBullets);

        S_B(triggerbot); S_I(triggerKey); S_F(triggerDelay);

        S_B(noRecoil); S_B(noSpread); S_B(infAmmo); S_B(noReload); S_B(rapidFire);

        S_B(speedHack); S_F(speedValue); S_B(fly); S_F(flySpeed);
        S_B(superJump); S_F(jumpPower); S_B(teleport);

        S_B(godmode); S_B(hitmarker); S_F(camFov);
        S_B(crosshair); S_F(crosshairSize);
        WCol(f, "crosshairColor", g_cfg.crosshairColor);
        S_B(watermark);

        S_B(antiCheat); S_B(blockedNoZone); S_B(blockedUnlock); S_I(stage);

#undef S_B
#undef S_I
#undef S_F

        fclose(f);
        Log("[cfg] salvo em %s", CfgPath());
    }

    void Load() {
        FILE* f = nullptr;
        if (fopen_s(&f, CfgPath(), "r") != 0 || !f) { Log("[cfg] sem arquivo, usando padroes"); return; }

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char* nl = strpbrk(line, "\r\n");
            if (nl) *nl = 0;
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char* k = line;
            const char* v = eq + 1;

#define L_B(x) else if (!strcmp(k, #x)) g_cfg.x = (atoi(v) != 0)
#define L_I(x) else if (!strcmp(k, #x)) g_cfg.x = atoi(v)
#define L_F(x) else if (!strcmp(k, #x)) g_cfg.x = (float)atof(v)
#define L_C(x) else if (!strcmp(k, #x)) RCol(v, g_cfg.x)

            if (0) {}

            L_B(menuOpen); L_I(menuKey); L_I(panicKey); L_I(saveKey); L_I(unloadKey);

            L_B(esp); L_B(espBox); L_B(espFill); L_B(espName); L_B(espHealth);
            L_B(espHealthNum); L_B(espWeapon); L_B(espDistance); L_B(espSnapline);
            L_B(espSkeleton); L_B(espNpc); L_B(teamCheck); L_B(fps);
            L_F(espThickness); L_F(espMaxDist); L_F(espFontSize);
            L_F(espOffY); L_F(espScaleY); L_F(espBodyH);
            L_C(espColor); L_C(espBoxColor); L_C(espTextColor); L_C(espSkelColor);

            L_B(aimbot); L_I(aimKey); L_F(aimFov); L_F(aimSmooth); L_I(aimBone);
            L_I(aimMode); L_B(aimVisible); L_B(aimTeam); L_B(aimRotate);
            L_B(showFovCircle); L_F(aimMaxDist);
            L_C(aimColor);

            L_B(silentAim); L_B(magicBullets);

            L_B(triggerbot); L_I(triggerKey); L_F(triggerDelay);

            L_B(noRecoil); L_B(noSpread); L_B(infAmmo); L_B(noReload); L_B(rapidFire);

            L_B(speedHack); L_F(speedValue); L_B(fly); L_F(flySpeed);
            L_B(superJump); L_F(jumpPower); L_B(teleport);

            L_B(godmode); L_B(hitmarker); L_F(camFov);
            L_B(crosshair); L_F(crosshairSize);
            L_C(crosshairColor);
            L_B(watermark);

            L_B(antiCheat); L_B(blockedNoZone); L_B(blockedUnlock); L_I(stage);

#undef L_B
#undef L_I
#undef L_F
#undef L_C
        }
        fclose(f);
        Log("[cfg] carregado de %s", CfgPath());
    }

    void Init() { Load(); }

} // namespace ConfigIO