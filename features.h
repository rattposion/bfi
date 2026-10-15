#pragma once
#include "sdk.h"
#include "config.h"

namespace ESP { void Draw(); }
namespace Visuals { void Draw(); }
namespace Aimbot { bool Install(); void Tick(); Il2CppObject* Best(); Vec3 Bone(Il2CppObject* p); Vec3 Origin(); void WriteAim(Il2CppObject* target); }
namespace Triggerbot { bool Install(); void Tick(); }
namespace Combat { bool Install(); void Tick(); }
namespace Movement { void Tick(); }
namespace Weapons { void Tick(); }
namespace Misc { void Tick(); void Draw(); void Panic(); }
namespace AntiCheat { bool Install(); }

void MenuDraw();