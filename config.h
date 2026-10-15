#pragma once
#include "common.h"

struct Config {
    // esp
    bool  esp = true, espBox = true, espHealth = true, espName = true;
    bool  espSnapline = false, espDistance = true, espWeapon = false, espHealthNum = false;
    bool  teamCheck = false;
    float espColor[4] = { 0.f, 1.f, 0.f, 1.f };
    float espOffY = 0.f, espScaleY = 1.f, espBodyH = 1.70f;

    // aim
    bool  aimbot = false, aimVisible = true, aimPredict = false, aimRotate = true;
    float aimFov = 150.f, aimSmooth = 6.f, aimPredictMs = 60.f, aimDeadzone = 1.5f;
    int   aimBone = 0, aimMode = 0;
    bool  aimKeyDown = false;
    bool  showFovCircle = true;

    // combate
    bool  silentAim = false; int silentDirIndex = 1;
    bool  magicBullets = false; int magicIndex = 2; float magicRadius = 2.0f;
    bool  noRecoil = true;
    bool  triggerbot = false; float triggerDelay = 30.f;
    bool  godmode = false, damageMult = false; float damageValue = 2.0f;

    // armas
    bool infAmmo = false, noReload = false, rapidFire = false, noSpread = false;

    // movimento
    bool  speedHack = false; float speedValue = 1.6f;
    bool  superJump = false, fly = false, teleport = false;

    // hud
    bool watermark = true, crosshair = true, radar = false, hitmarker = true;
    float camFov = 0.f;                 // 0 = nao mexe

    bool antiCheat = true;
    int  blockedNoZone = 0, blockedUnlock = 0;

    int  menuKey = VK_INSERT, panicKey = VK_DELETE, saveKey = VK_F5, unloadKey = VK_F8, aimKey = VK_RBUTTON;
    int  stage = 3;
    bool menuOpen = true;
};

extern Config C;

namespace f {
    extern std::atomic<ULONGLONG> lastHitTime;
}

namespace ConfigIO { void Init(); void Save(); void Load(); }