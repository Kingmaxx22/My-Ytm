#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace myytm::platform {

class ICrypto {
public:
    virtual ~ICrypto() = default;
    // DPAPI protect/unprotect — isolated per AGENTS.md. Never logs secrets.
    virtual bool protect(const std::string& plain, std::string& out) = 0;
    virtual bool unprotect(const std::string& enc, std::string& out) = 0;

    // Secure random — cryptographically strong (BCryptGenRandom). Never logs output.
    virtual bool secureRandom(std::vector<uint8_t>& out, size_t len) = 0;
    // SHA-256 via BCrypt — returns 32-byte digest. Never logs input/output.
    virtual bool sha256(const std::string& data, std::vector<uint8_t>& out) = 0;
};

std::unique_ptr<ICrypto> makeCrypto();

// Helpers — keep Windows/Bcrypt details in CryptoWin.cpp, no secrets logged.
std::string base64UrlEncode(const std::vector<uint8_t>& data); // no '=' padding
std::string base64UrlEncode(const std::string& s);
std::string generateSecureState(size_t randomBytes = 32); // base64url, ~43 chars
std::string generatePkceVerifier(); // 43 chars, base64url of 32 random bytes
std::string computePkceChallenge(const std::string& verifier); // S256: base64url(SHA256(verifier))

} // namespace myytm::platform
