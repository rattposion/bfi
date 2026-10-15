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

    bool  fps = false;

    // ---------------- Aimbot ----------------
    bool  aimbot = false;
    int   aimKey = VK_XBUTTON2;
    float aimFov = 8.f;
    float aimSmooth = 5.f;
    int   aimBone = 0;          // 0 = cabeca, 1 = pescoco, 2 = peito
    int   aimMode = 0;          // 0 = mais perto da mira, 1 = menor vida, 2 = mais perto
    bool  aimVisible = true;
    bool  aimTeam = false;
    bool  aimRotate = true;
    bool  showFovCircle = false;
    float aimMaxDist = 250.f;
    float aimColor[4] = { 0.f, 1.f, 0.f, 1.f };

    // ---------------- Silent / magic ----------------
    bool  silentAim = false;
    bool  magicBullets = false;

    // ---------------- Triggerbot ----------------
    bool  triggerbot = false;
    int   triggerKey = VK_MENU;
    float triggerDelay = 30.f;

    // ---------------- Armas ----------------
    bool  noRecoil = true;
    bool  noSpread = false;
    bool  infAmmo = false;
    bool  noReload = false;
    bool  rapidFire = false;

    // ---------------- Movimento ----------------
    bool  speedHack = false;
    float speedValue = 8.f;
    bool  fly = false;
    float flySpeed = 6.f;
    bool  superJump = false;
    float jumpPower = 6.f;
    bool  teleport = false;

    // ---------------- Misc ----------------
    bool  godmode = false;
    bool  hitmarker = false;
    float camFov = 90.f;
    bool  crosshair = false;
    float crosshairSize = 6.f;
    float crosshairColor[4] = { 0.f, 1.f, 0.f, 1.f };
    bool  watermark = true;

    // ---------------- Anticheat ----------------
    bool  antiCheat = false;
    bool  blockedNoZone = false;
    bool  blockedUnlock = false;
    int   stage = 0;

    // ---------------- runtime (nao persistente) ----------------
    bool  aimKeyDown = false;
    float lastHitTime = 0.f;
};

extern Config  g_cfg;
extern Config& C;

namespace ConfigIO {
    void Init();
    void Save();
    void Load();
}