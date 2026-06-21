#include "crypto.h"
#include "config.h"
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

namespace Crypto {
    std::vector<BYTE> g_sessionKey;
    std::string g_uuid;

    HCRYPTPROV hProv = 0;
    HCRYPTKEY hRSAKey = 0;
    HCRYPTKEY hAESKey = 0;

    static bool ImportAESKey(const std::vector<BYTE>& key) {
        if (hAESKey) {
            CryptDestroyKey(hAESKey);
            hAESKey = 0;
        }
        if (key.size() != AES_KEY_SIZE) return false;

        struct {
            BLOBHEADER header;
            DWORD keyLen;
            BYTE keyData[AES_KEY_SIZE];
        } blob;
        blob.header.bType = PLAINTEXTKEYBLOB;
        blob.header.bVersion = CUR_BLOB_VERSION;
        blob.header.reserved = 0;
        blob.header.aiKeyAlg = CALG_AES_256;
        blob.keyLen = AES_KEY_SIZE;
        memcpy(blob.keyData, key.data(), AES_KEY_SIZE);

        if (!CryptImportKey(hProv, (BYTE*)&blob, sizeof(blob), 0, 0, &hAESKey))
            return false;
        g_sessionKey = key;
        return true;
    }

    bool Init() {
        if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES,
                                   CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
            return false;

        DWORD flags = (RSA_KEY_SIZE << 16) | CRYPT_EXPORTABLE;
        if (!CryptGenKey(hProv, AT_KEYEXCHANGE, flags, &hRSAKey))
            return false;

        g_sessionKey = RandomBytes(AES_KEY_SIZE);
        return ImportAESKey(g_sessionKey);
    }

    std::vector<BYTE> RandomBytes(size_t len) {
        std::vector<BYTE> buf(len);
        CryptGenRandom(hProv, (DWORD)len, buf.data());
        return buf;
    }

    std::vector<BYTE> AESEncrypt(const std::vector<BYTE>& plaintext,
                                 const std::vector<BYTE>& key,
                                 std::vector<BYTE>& iv) {
        std::vector<BYTE> result;
        if (key.size() != AES_KEY_SIZE || plaintext.empty()) return result;

        if (key != g_sessionKey || !hAESKey) {
            if (!ImportAESKey(key)) return result;
        }

        iv = RandomBytes(16);
        CryptSetKeyParam(hAESKey, KP_IV, iv.data(), 0);

        DWORD len = (DWORD)plaintext.size();
        DWORD blockLen = ((len + 15) / 16) * 16;
        if (blockLen == 0) blockLen = 16;
        result.resize(blockLen);
        memcpy(result.data(), plaintext.data(), len);

        if (!CryptEncrypt(hAESKey, 0, TRUE, 0, result.data(), &len, blockLen)) {
            result.clear();
            return result;
        }
        result.resize(len);
        return result;
    }

    std::vector<BYTE> AESDecrypt(const std::vector<BYTE>& ciphertext,
                                 const std::vector<BYTE>& key,
                                 const std::vector<BYTE>& iv) {
        std::vector<BYTE> result;
        if (key.size() != AES_KEY_SIZE || ciphertext.empty()) return result;

        if (key != g_sessionKey || !hAESKey) {
            if (!ImportAESKey(key)) return result;
        }

        CryptSetKeyParam(hAESKey, KP_IV, iv.data(), 0);

        result = ciphertext;
        DWORD len = (DWORD)result.size();
        if (!CryptDecrypt(hAESKey, 0, TRUE, 0, result.data(), &len)) {
            result.clear();
            return result;
        }
        result.resize(len);
        return result;
    }

    std::vector<BYTE> RSAEncrypt(const std::vector<BYTE>& data) {
        DWORD len = (DWORD)data.size();
        DWORD blockLen = len;
        std::vector<BYTE> result(len);
        memcpy(result.data(), data.data(), len);

        if (!CryptEncrypt(hRSAKey, 0, TRUE, 0, result.data(), &len, blockLen)) {
            DWORD needed = len;
            result.resize(needed);
            memcpy(result.data(), data.data(), data.size());
            if (!CryptEncrypt(hRSAKey, 0, TRUE, 0, result.data(), &len, needed)) {
                return {};
            }
        }
        result.resize(len);
        return result;
    }

    std::vector<BYTE> RSADecrypt(const std::vector<BYTE>& data) {
        DWORD len = (DWORD)data.size();
        std::vector<BYTE> result = data;
        if (!CryptDecrypt(hRSAKey, 0, TRUE, 0, result.data(), &len)) {
            return {};
        }
        result.resize(len);
        return result;
    }

    std::string GetPublicKeyPEM() {
        DWORD len = 0;
        CryptExportKey(hRSAKey, 0, PUBLICKEYBLOB, 0, NULL, &len);
        std::vector<BYTE> blob(len);
        CryptExportKey(hRSAKey, 0, PUBLICKEYBLOB, 0, blob.data(), &len);
        blob.resize(len);

        std::string b64 = Base64Encode(blob);
        std::string pem = "-----BEGIN PUBLIC KEY-----\n";
        for (size_t i = 0; i < b64.size(); i += 64) {
            pem += b64.substr(i, 64) + "\n";
        }
        pem += "-----END PUBLIC KEY-----\n";
        return pem;
    }

    std::string Base64Encode(const std::vector<BYTE>& data) {
        DWORD len = 0;
        CryptBinaryToStringA(data.data(), (DWORD)data.size(),
                             CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &len);
        std::string result(len, 0);
        CryptBinaryToStringA(data.data(), (DWORD)data.size(),
                             CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                             &result[0], &len);
        result.resize(len - 1);
        return result;
    }

    std::vector<BYTE> Base64Decode(const std::string& str) {
        DWORD len = 0;
        CryptStringToBinaryA(str.c_str(), (DWORD)str.size(),
                             CRYPT_STRING_BASE64, NULL, &len, NULL, NULL);
        std::vector<BYTE> result(len);
        CryptStringToBinaryA(str.c_str(), (DWORD)str.size(),
                             CRYPT_STRING_BASE64, result.data(), &len, NULL, NULL);
        result.resize(len);
        return result;
    }

    std::vector<BYTE> SHA256(const std::vector<BYTE>& data) {
        HCRYPTHASH hHash = 0;
        std::vector<BYTE> result(32);
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, data.data(), (DWORD)data.size(), 0);
            DWORD len = 32;
            CryptGetHashParam(hHash, HP_HASHVAL, result.data(), &len, 0);
            CryptDestroyHash(hHash);
            result.resize(len);
        }
        return result;
    }
}
