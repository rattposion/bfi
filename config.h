#pragma once
#include "common.h"

struct Config {
    bool esp = true, espBox = true, espHealth = true, espName = true;
    bool espSnapline = false, espDistance = true, espWeapon = false;
    bool espHealthNum = false;
    bool teamCheck = false;
    float espColor[4] = { 0.f, 1.f, 0.f, 1.f };
    float camFov = 60.f;
    float espOffY = 0.f;
    float espScaleY = 1.f;
    float espBodyH = 1.70f;

    bool aimbot = false, aimVisible = true, aimPredict = false;
    bool aimRotate = true;
    float aimFov = 150.f, aimSmooth = 6.f, aimPredictMs = 60.f;
    float aimDeadzone = 1.5f;
    int  aimBone = 0;
    int  aimMode = 0;
    bool aimKeyDown = false;         // runtime: tecla do aim apertada

    bool silentAim = false; int silentDirIndex = 1;
    bool magicBullets = false; int magicIndex = 2; float magicRadius = 2.0f;
    bool noRecoil = true;

    bool triggerbot = false; float triggerDelay = 30.f;

    bool godmode = false, damageMult = false; float damageValue = 2.0f;
    bool noFlinch = false;

    bool infAmmo = false, noReload = false, rapidFire = false, noSpread = false;

    bool speedHack = false; float speedValue = 1.6f;
    bool superJump = false, fly = false, teleport = false;

    bool watermark = true, crosshair = true, radar = false, hitmarker = true;

    bool antiCheat = true;
    int  blockedNoZone = 0, blockedUnlock = 0;

    int  menuKey = VK_INSERT, panicKey = VK_DELETE, aimKey = VK_RBUTTON;
    int  stage = 3;
    bool menuOpen = true;

    bool showFovCircle = true;       // desenhar circulo de FOV
};

extern Config C;
namespace ConfigIO { void Init(); void Save(); void Load(); }