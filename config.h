#pragma once
#ifndef BFI_CONFIG_H
#define BFI_CONFIG_H

#include <windows.h>

struct Config {

    // ---- JANELA / MENU ----
    bool  menuOpen = true;
    int   menuKey = VK_INSERT;
    int   panicKey = VK_DELETE;
    int   saveKey = VK_F5;
    int   unloadKey = VK_F8;

    // ---- ESP ----
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

    bool  espVisCheck = true;
    bool  espVisLine = true;
    float visColor[4] = { 0.f, 1.f, 0.f, 1.f };
    float noVisColor[4] = { 1.f, 0.f, 0.f, 1.f };

    bool  fps = false;

    // ---- PERFORMANCE ----
    // LOS: quantos alvos podem ser testados por frame (rodizio).
    //   8 = equilibrio bom.  0 = sem teto (todos, todo frame -> o que travava).
    // Cada alvo e' reavaliado a cada ceil(nAlvos / losPerFrame) frames.
    int   losPerFrame = 8;

    // ---- AIMBOT ----
    bool  aimbot = false;
    int   aimKey = VK_RBUTTON;
    float aimFov = 8.f;
    float aimSmooth = 5.f;
    int   aimBone = 0;
    int   aimMode = 0;
    bool  aimVisible = true;

    // Checar visao (LOS):
    //   MARCADO   -> mira SOMENTE em quem esta visivel. Se ninguem estiver
    //                visivel, NAO mira em ninguem (sem fallback).
    //   DESMARCADO-> sem filtro, mira atraves da parede.
    bool  aimVisCheck = true;

    // Nao mirar atras do muro:
    //   true  -> se ninguem estiver visivel, NAO mira em ninguem (padrao).
    //   false -> cai no mais proximo da mira (volta a mirar no muro).
    bool  aimStrictVis = true;

    bool  aimTeam = false;
    bool  aimRotate = true;
    bool  showFovCircle = false;
    float aimMaxDist = 250.f;
    float aimColor[4] = { 0.f, 1.f, 0.f, 1.f };

    bool  silentAim = false;
    bool  magicBullets = false;

    // ---- TRIGGERBOT ----
    bool  triggerbot = false;
    int   triggerKey = VK_MENU;
    float triggerDelay = 30.f;

    // ---- ARMA ----
    bool  noRecoil = true;
    bool  noSpread = false;
    bool  infAmmo = false;
    bool  noReload = false;
    bool  rapidFire = false;

    // ---- MOVIMENTO ----
    bool  speedHack = false;
    float speedValue = 8.f;
    bool  fly = false;
    float flySpeed = 6.f;
    bool  superJump = false;
    float jumpPower = 6.f;
    bool  teleport = false;

    // ---- MISC ----
    bool  godmode = false;
    bool  hitmarker = false;
    float camFov = 90.f;
    bool  crosshair = false;
    float crosshairSize = 6.f;
    float crosshairColor[4] = { 0.f, 1.f, 0.f, 1.f };
    bool  watermark = true;

    // ---- ANTICHEAT ----
    bool  antiCheat = true;
    bool  blockedNoZone = false;
    bool  blockedUnlock = false;
    int   stage = 0;

    // ---- UNLOCK ALL ----
    bool  unlockAll = true;
    bool  unlockWeapons = true;
    bool  unlockSkins = true;
    bool  unlockAttachments = true;
    bool  unlockCurrency = true;
    bool  fillLoadoutSlots = false;
    bool  applySlots = false;
    int   slotWeapon[3] = { -1, -1, -1 };
    int   currencyValue = 999999;
    float unlockInterval = 0.5f;
    int   unlockLevel = 999;

    // ---- NOMES (arma / skin / acessorio) ----
    bool  forceNames = false;
    char  weaponName[32] = "";
    char  skinName[32] = "";
    char  attachName[32] = "";

    // ---- SKIN CHANGER (campos int, se existirem) ----
    bool  skinChanger = false;
    int   skinIndex = 0;

    bool  aimKeyDown = false;
    float lastHitTime = 0.f;

    // =======================================================================
    //  SELECAO DO LOADOUT / SKINS / PERSONAGENS
    //  Os indices sao os das listas Unlock::WeaponList() / CamoList() /
    //  CharList() / AttachList(slot).  -1 = "nao mexer".
    // =======================================================================
    bool  unlockChars = true;               // destrava personagens
    bool  applySelection = true;            // aplica o que voce escolheu abaixo
    int   selWeapon = -1;                   // indice em Unlock::WeaponList()
    int   selCamo = -1;                     // indice em Unlock::CamoList()
    int   selChar = -1;                     // indice em Unlock::CharList()
    int   selAttach[3] = { -1, -1, -1 };    // 0=sights 1=accessories 2=barrels
    int   loadoutSlot = 0;                  // slot do loadout (0..2) que recebe selWeapon
    char  charName[32] = "";                // nome do personagem escolhido

    // =======================================================================
    //  LOG / DUMP  (campos novos ficam no FIM de proposito)
    // =======================================================================
    bool  probeLog = false;                 // [watch] / [probe] / [fire7] / [aim]
    bool  beatLog = true;                   // "[hooks] beat: ..." a cada 2s
    bool  verboseUnlock = true;
    int   watchMaxLines = 12;

    bool  dumpOnce = true;
    int   dumpIntervalMs = 5000;

    bool  gunCustomizeUnlock = true;
    bool  gunCustomizeAllSlots = true;

    bool  showAimTargetName = false;
    bool  showFpsGraph = false;
};

extern Config  g_cfg;

#ifdef C
#undef C
#endif
extern Config& C;

// C.menuOpen  -> funciona (macro function-like so expande com parenteses)
// C(menuOpen) -> g_cfg.menuOpen
#define C(f) g_cfg.f

namespace ConfigIO {
    void Init();
    void Save();
    void Load();
}

#endif // BFI_CONFIG_H