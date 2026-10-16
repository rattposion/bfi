#pragma once

namespace Unlock {
    bool Install();
    void Tick();
    void Dump();

    const char* const* CamoList();
    int CamoCount();
}