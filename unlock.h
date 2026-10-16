#pragma once
#include "common.h"

// API do Unlock usada pelo menu. As listas devolvem ponteiros ESTAVEIS
// (montadas uma vez e nunca realocadas), pode guardar sem risco.
namespace Unlock {
    bool Install();
    void Tick();
    void Dump();

    int WeaponCount();             // quantidade de armas do loadout
    const char* const* WeaponList();          // "00 - AK47", ...

    int AttachSlotCount();                    // 3 (sights / accessories / barrels)
    const char* AttachSlotName(int slot);
    int AttachCount(int slot);
    const char* const* AttachList(int slot);
    int AttachValue(int slot, int idx);       // valor real do enum daquele anexo

    int CamoCount();
    const char* const* CamoList();

    int CharCount();
    const char* const* CharList();
}