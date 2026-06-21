#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Crypto {
    // Initialize: generate RSA keypair, derive AES session key
    bool Init();
    
    // AES-256-CBC encrypt/decrypt
    std::vector<BYTE> AESEncrypt(const std::vector<BYTE>& plaintext, const std::vector<BYTE>& key, std::vector<BYTE>& iv);
    std::vector<BYTE> AESDecrypt(const std::vector<BYTE>& ciphertext, const std::vector<BYTE>& key, const std::vector<BYTE>& iv);
    
    // RSA encrypt/decrypt (for session key exchange)
    std::vector<BYTE> RSAEncrypt(const std::vector<BYTE>& data);
    std::vector<BYTE> RSADecrypt(const std::vector<BYTE>& data);
    
    // Get public key PEM for registration
    std::string GetPublicKeyPEM();
    
    // Random bytes
    std::vector<BYTE> RandomBytes(size_t len);
    
    // Base64 encode/decode
    std::string Base64Encode(const std::vector<BYTE>& data);
    std::vector<BYTE> Base64Decode(const std::string& str);
    
    // Hash (SHA-256)
    std::vector<BYTE> SHA256(const std::vector<BYTE>& data);
    
    // Session management
    extern std::vector<BYTE> g_sessionKey;
    extern std::string g_uuid;
}