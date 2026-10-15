#include "config.h"

Config C;

namespace f { std::atomic<ULONGLONG> lastHitTime{ 0 }; }

struct CV { const char* n; int t; void* p; };
static std::vector<CV> g_vars;
static std::string g_path;

namespace ConfigIO {

    void Init() {
        char b[MAX_PATH]{};
        GetModuleFileNameA(g_hModule, b, MAX_PATH);
        g_path = std::string(b);
        g_path = g_path.substr(0, g_path.find_last_of("\\/")) + "\\BF_config.ini";

#define B(x) g_vars.push_back({#x,0,&C.x})
#define F(x) g_vars.push_back({#x,1,&C.x})
#define I(x) g_vars.push_back({#x,2,&C.x})

        B(esp); B(espBox); B(espHealth); B(espHealthNum); B(espName);
        B(espSnapline); B(espDistance); B(espWeapon); B(teamCheck);
        F(espColor[0]); F(espColor[1]); F(espColor[2]); F(espColor[3]);
        F(espOffY); F(espScaleY); F(espBodyH);
        B(aimbot); B(aimVisible); B(aimPredict); B(aimRotate);
        F(aimFov); F(aimSmooth); F(aimPredictMs); F(aimDeadzone);
        I(aimBone); I(aimMode); B(showFovCircle);
        B(silentAim); I(silentDirIndex); B(magicBullets); I(magicIndex); F(magicRadius);
        B(noRecoil); B(triggerbot); F(triggerDelay);
        B(godmode); B(damageMult); F(damageValue);
        B(infAmmo); B(noReload); B(rapidFire); B(noSpread);
        B(speedHack); F(speedValue); B(superJump); B(fly); B(teleport);
        B(watermark); B(crosshair); B(radar); B(hitmarker); F(camFov);
        B(antiCheat); I(blockedNoZone); I(blockedUnlock);
        I(menuKey); I(panicKey); I(saveKey); I(unloadKey); I(aimKey); I(stage);
        B(menuOpen);

#undef B
#undef F
#undef I

        Load();
    }

    void Save() {
        FILE* f = nullptr;
        fopen_s(&f, g_path.c_str(), "w");
        if (!f) { Log("config: falha ao salvar %s", g_path.c_str()); return; }
        for (auto& v : g_vars) {
            if (v.t == 0)      fprintf(f, "%s=%d\n", v.n, *(bool*)v.p ? 1 : 0);
            else if (v.t == 1) fprintf(f, "%s=%.4f\n", v.n, *(float*)v.p);
            else               fprintf(f, "%s=%d\n", v.n, *(int*)v.p);
        }
        fclose(f);
        Log("config salva em %s", g_path.c_str());
    }

    void Load() {
        FILE* f = nullptr;
        fopen_s(&f, g_path.c_str(), "r");
        if (!f) return;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            std::string k = line;
            float val = (float)atof(eq + 1);
            for (auto& v : g_vars) {
                if (k != v.n) continue;
                if (v.t == 0)      *(bool*)v.p = (val != 0);
                else if (v.t == 1) *(float*)v.p = val;
                else               *(int*)v.p = (int)val;
            }
        }
        fclose(f);

        if (C.aimBone < 0 || C.aimBone > 2)              C.aimBone = 0;
        if (C.aimMode < 0 || C.aimMode > 3)              C.aimMode = 0;
        if (C.silentDirIndex < 0 || C.silentDirIndex > 6) C.silentDirIndex = 1;
        if (C.magicIndex < 0 || C.magicIndex > 6)        C.magicIndex = 2;
        if (C.aimFov < 20.f || C.aimFov > 600.f)         C.aimFov = 150.f;
        if (C.aimSmooth < 1.f || C.aimSmooth > 20.f)     C.aimSmooth = 6.f;
        if (C.aimDeadzone < 0.f || C.aimDeadzone > 10.f) C.aimDeadzone = 1.5f;
        if (C.camFov < 0.f || C.camFov > 140.f)          C.camFov = 0.f;
        if (C.espBodyH < 0.8f || C.espBodyH > 2.5f)      C.espBodyH = 1.70f;
        if (C.stage < 1 || C.stage > 3)                  C.stage = 3;
        if (C.espOffY < -300.f || C.espOffY > 300.f)     C.espOffY = 0.f;
        if (C.espScaleY < 0.5f || C.espScaleY > 2.f)     C.espScaleY = 1.f;
    }
}