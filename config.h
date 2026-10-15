#pragma once

#include "common.h"
#include <windows.h>

struct Config {

    // ---------------- menu / teclas ----------------
    bool  menuOpen = true;
    int   menuKey = VK_INSERT;
    int   panicKey = VK_DELETE;
    int   saveKey = VK_F5;
    int   unloadKey = VK_F8;

    // ---------------- ESP ----------------
    bool  esp = true;
    bool  espBox = true;
    bool  espFill = false;
    bool  espName = true;
    bool  espHealth = true;
    bool  espHealthNum = true;
    bool  espWeapon = false;
    bool  espDistance = true;
    bool  espSnapline = false;
    bool  espSkeleton = false;
    bool  espNpc = false;
    bool  teamCheck = false;
    float espThickness = 1.5f;
    float espMaxDist = 300.f;
    float espFontSize = 13.f;
    float espOffY = 0.f;
    float espScaleY = 1.f;
    float espBodyH = 1.8f;
    float espColor[4] = { 1.f, 0.15f, 0.15f, 1.f };
    float espBoxColor[4] = { 1.f, 1.f, 1.f, 1.f };
    float espTextColor[4] = { 1.f, 1.f, 1.f, 1.f };
    float espSkelColor[4] = { 1.f, 1.f, 0.f, 1.f };
    bool  speed = false;
    bool  infiniteAmmo = false;
    bool  fps = false;
    bool  espLine = false;
    bool  espDist = false;
    // ---------------- Aimbot ----------------
    bool  aimbot = false;
    int   aimKey = VK_XBUTTON2;
    float aimFov = 8.f;
    float aimSmooth = 5.f;
    int   aimBone = 0;
    int   aimMode = 0;
    bool  aimVisible = true;
    bool  aimTeam = false;
    bool  aimPredict = false;
    bool  aimRotate = true;
    bool  aimAutoFire = false;
    bool  showFovCircle = false;
    float aimPredictMs = 0.f;
    float aimDeadzone = 0.5f;
    float aimMaxDist = 250.f;
    float aimColor[4] = { 0.f, 1.f, 0.f, 1.f };

    // ---------------- Silent / magic ----------------
    bool  silentAim = false;
    int   silentKey = 0;
    bool  silentVisible = false;
    bool  magicBullets = false;
    int   magicIndex = 1;
    float magicRadius = 1.f;
    float silentDirIndex = 0.f;

    // ---------------- Triggerbot ----------------
    bool  triggerbot = false;
    int   triggerKey = VK_MENU;
    float triggerDelay = 30.f;
    float triggerFov = 3.f;
    float triggerDist = 200.f;
    bool  triggerBurst = false;

    // ---------------- Armas ----------------
    bool  noRecoil = true;
    bool  noSpread = false;
    bool  infAmmo = false;
    bool  noReload = false;
    bool  rapidFire = false;
    bool  damageMult = false;
    float damageValue = 2.f;
    float spreadValue = 0.f;
    float fireRateMult = 2.f;

    // ---------------- Movimento ----------------
    bool  speedHack = false;
    float speedValue = 8.f;
    bool  fly = false;
    float flySpeed = 12.f;
    int   flyKey = VK_SPACE;
    bool  noGravity = false;
    bool  bunnyHop = false;
    bool  superJump = false;
    bool  teleport = false;
    bool  infiniteJump = false;
    float jumpPower = 6.f;

    // ---------------- Misc ----------------
    bool  godmode = false;
    bool  hitmarker = false;
    float fov = 90.f;
    float camFov = 90.f;
    bool  fovEnabled = false;
    bool  thirdPerson = false;
    float thirdPersonDist = 3.f;
    bool  noFog = false;
    bool  fullBright = false;
    bool  crosshair = false;
    float crosshairSize = 6.f;
    float crosshairColor[4] = { 0.f, 1.f, 0.f, 1.f };
    bool  watermark = true;

    // ---------------- Radar ----------------
    bool  radar = false;
    float radarSize = 160.f;
    float radarPosX = 20.f;
    float radarPosY = 20.f;
    bool  radarRotate = true;
    float radarColor[4] = { 1.f, 1.f, 1.f, 1.f };

    // ---------------- Anticheat ----------------
    bool  antiCheat = false;
    bool  antiHackBypass = false;
    bool  spoofName = false;
    bool  blockedNoZone = false;
    bool  blockedUnlock = false;
    int   stage = 0;
    char  fakeName[32] = "player";
    bool  aimKeyDown = false;
    float lastHitTime = 0.f;
    float hitmarkerTime = 0.f;
};

extern Config  g_cfg;
extern Config& C;

namespace ConfigIO {
    void Init();
    void Save();
    void Load();
}