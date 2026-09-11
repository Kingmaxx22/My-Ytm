#include "platform/crypto/ICrypto.h"
#include "platform/win32_error.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#endif
#include <vector>

namespace myytm::platform {

class CryptoWin final : public ICrypto {
public:
    bool protect(const std::string& plain, std::string& out) override {
#ifdef _WIN32
        DATA_BLOB in{}, outBlob{};
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
        in.cbData = static_cast<DWORD>(plain.size());
        if (!CryptProtectData(&in, L"MyYtm", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) return false;
        out.assign(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);
        LocalFree(outBlob.pbData);
        return true;
#else
        (void)plain; (void)out; return false;
#endif
    }
    bool unprotect(const std::string& enc, std::string& out) override {
#ifdef _WIN32
        DATA_BLOB in{}, outBlob{};
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(enc.data()));
        in.cbData = static_cast<DWORD>(enc.size());
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) return false;
        out.assign(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);
        LocalFree(outBlob.pbData);
        return true;
#else
        (void)enc; (void)out; return false;
#endif
    }
    bool secureRandom(std::vector<uint8_t>& out, size_t len) override {
#ifdef _WIN32
        out.resize(len);
        NTSTATUS st = BCryptGenRandom(nullptr, out.data(), (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (st == 0) return true;
        // Fallback to CryptGenRandom
        HCRYPTPROV hProv = 0;
        if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return false;
        BOOL ok = CryptGenRandom(hProv, (DWORD)len, out.data());
        CryptReleaseContext(hProv, 0);
        return ok != 0;
#else
        (void)out; (void)len; return false;
#endif
    }
    bool sha256(const std::string& data, std::vector<uint8_t>& out) override {
#ifdef _WIN32
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (st != 0) return false;
        DWORD hashLen = 0; DWORD retLen = 0;
        st = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &retLen, 0);
        if (st != 0) { BCryptCloseAlgorithmProvider(hAlg,0); return false; }
        BCRYPT_HASH_HANDLE hHash = nullptr;
        st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        if (st != 0) { BCryptCloseAlgorithmProvider(hAlg,0); return false; }
        st = BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0);
        if (st != 0) { BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg,0); return false; }
        out.resize(hashLen);
        st = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg,0);
        return st == 0;
#else
        (void)data; (void)out; return false;
#endif
    }
};

std::unique_ptr<ICrypto> makeCrypto() {
    return std::make_unique<CryptoWin>();
}

std::string base64UrlEncode(const std::vector<uint8_t>& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size()+2)/3)*4);
    for (size_t i=0;i<data.size();) {
        uint32_t a = i < data.size() ? data[i++] : 0;
        uint32_t b = i < data.size() ? data[i++] : 0;
        uint32_t c = i < data.size() ? data[i++] : 0;
        uint32_t triple = (a<<16) | (b<<8) | c;
        out.push_back(tbl[(triple>>18)&0x3F]);
        out.push_back(tbl[(triple>>12)&0x3F]);
        out.push_back(tbl[(triple>>6)&0x3F]);
        out.push_back(tbl[triple&0x3F]);
    }
    size_t mod = data.size() % 3;
    if (mod == 1) { out.pop_back(); out.pop_back(); }
    else if (mod == 2) { out.pop_back(); }
    for (char& ch : out) {
        if (ch == '+') ch = '-';
        else if (ch == '/') ch = '_';
    }
    return out;
}

std::string base64UrlEncode(const std::string& s) {
    std::vector<uint8_t> v(s.begin(), s.end());
    return base64UrlEncode(v);
}

std::string generateSecureState(size_t randomBytes) {
    auto crypto = makeCrypto();
    std::vector<uint8_t> buf;
    if (!crypto->secureRandom(buf, randomBytes)) return "";
    return base64UrlEncode(buf);
}

std::string generatePkceVerifier() {
    // RFC7636: 43-128 chars, use 32 random bytes -> 43 base64url chars (minimum)
    return generateSecureState(32);
}

std::string computePkceChallenge(const std::string& verifier) {
    auto crypto = makeCrypto();
    std::vector<uint8_t> hash;
    if (!crypto->sha256(verifier, hash)) return "";
    return base64UrlEncode(hash);
}

} // namespace myytm::platform
