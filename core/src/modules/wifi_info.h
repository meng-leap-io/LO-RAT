#pragma once
#include "../beacon.h"

namespace WifiInfo {
    void Scan();
    void OnCommand(const Beacon::Command& cmd);
}