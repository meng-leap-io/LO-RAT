#include "crypto.h"
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

namespace Crypto {
    std::vector<BYTE> g_sessionKey;
    std::string g_uuid;
    
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hRSAKey = 0;
    
    bool Init() {
        if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
            return false;
        
        // Generate RSA-2048 keypair
        DWORD flags = (RSA_KEY_SIZE << 16) | CRYPT_EXPORTABLE;
        if (!CryptGenKey(hProv, AT_KEYEXCHANGE, flags, &hRSAKey))
            return false;
        
        // Generate random session key (will be replaced by server-derived key after registration)
        g_sessionKey = RandomBytes(AES_KEY_SIZE);
        
        return true;
    }
    
    std::vector<BYTE> RandomBytes(size_t len) {
        std::vector<BYTE> buf(len);
        CryptGenRandom(hProv, (DWORD)len, buf.data());
        return buf;
    }
    
    std::vector<BYTE> AESEncrypt(const std::vector<BYTE>& plaintext, const std::vector<BYTE>& key, std::vector<BYTE>& iv) {
        std::vector<BYTE> result;
        if (key.size() != AES_KEY_SIZE) return result;
        
        HCRYPTKEY hKey = 0;
        HCRYPTHASH hHash = 0;
        
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) return result;
        CryptHashData(hHash, key.data(), (DWORD)key.size(), 0);
        CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey);
        
        iv = RandomBytes(16);
        
        DWORD len = (DWORD)plaintext.size();
        DWORD blockLen = ((len + 15) / 16) * 16;
        result.resize(blockLen);
        memcpy(result.data(), plaintext.data(), len);
        
        CryptEncrypt(hKey, 0, TRUE, 0, result.data(), &len, blockLen);
        
        CryptDestroyKey(hKey);
        CryptDestroyHash(hHash);
        return result;
    }
    
    std::vector<BYTE> AESDecrypt(const std::vector<BYTE>& ciphertext, const std::vector<BYTE>& key, const std::vector<BYTE>& iv) {
        std::vector<BYTE> result = ciphertext;
        if (key.size() != AES_KEY_SIZE) return result;
        
        HCRYPTKEY hKey = 0;
        HCRYPTHASH hHash = 0;
        
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) return result;
        CryptHashData(hHash, key.data(), (DWORD)key.size(), 0);
        CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey);
        
        DWORD len = (DWORD)result.size();
        CryptDecrypt(hKey, 0, TRUE, 0, result.data(), &len);
        result.resize(len);
        
        CryptDestroyKey(hKey);
        CryptDestroyHash(hHash);
        return result;
    }
    
    std::vector<BYTE> RSAEncrypt(const std::vector<BYTE>& data) {
        std::vector<BYTE> result;
        DWORD len = (DWORD)data.size();
        result.resize(len);
        memcpy(result.data(), data.data(), len);
        
        if (!CryptEncrypt(hRSAKey, 0, TRUE, 0, result.data(), &len, len)) {
            // Retry with larger buffer
            result.resize(len);
            memcpy(result.data(), data.data(), data.size());
            if (!CryptEncrypt(hRSAKey, 0, TRUE, 0, result.data(), &len, len))
                return std::vector<BYTE>();
        }
        result.resize(len);
        return result;
    }
    
    std::string Base64Encode(const std::vector<BYTE>& data) {
        DWORD len = 0;
        CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64, NULL, &len);
        std::string result(len, 0);
        CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64, &result[0], &len);
        result.resize(len - 1); // Remove null
        return result;
    }
    
    std::vector<BYTE> Base64Decode(const std::string& str) {
        DWORD len = 0;
        CryptStringToBinaryA(str.c_str(), (DWORD)str.size(), CRYPT_STRING_BASE64, NULL, &len, NULL, NULL);
        std::vector<BYTE> result(len);
        CryptStringToBinaryA(str.c_str(), (DWORD)str.size(), CRYPT_STRING_BASE64, result.data(), &len, NULL, NULL);
        result.resize(len);
        return result;
    }
    
    std::string GetPublicKeyPEM() {
        DWORD len = 0;
        CryptExportKey(hRSAKey, 0, PUBLICKEYBLOB, 0, NULL, &len);
        std::vector<BYTE> blob(len);
        CryptExportKey(hRSAKey, 0, PUBLICKEYBLOB, 0, blob.data(), &len);
        blob.resize(len);
        return Base64Encode(blob);
    }
}