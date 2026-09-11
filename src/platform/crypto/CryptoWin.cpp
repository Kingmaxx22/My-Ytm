#include "platform/crypto/ICrypto.h"
#include "platform/win32_error.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

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
};

std::unique_ptr<ICrypto> makeCrypto() {
    return std::make_unique<CryptoWin>();
}

} // namespace myytm::platform
