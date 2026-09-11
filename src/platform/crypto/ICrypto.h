#pragma once
#include <memory>
#include <string>

namespace myytm::platform {

class ICrypto {
public:
    virtual ~ICrypto() = default;
    // DPAPI protect/unprotect — isolated per AGENTS.md. Never logs secrets.
    virtual bool protect(const std::string& plain, std::string& out) = 0;
    virtual bool unprotect(const std::string& enc, std::string& out) = 0;
};

std::unique_ptr<ICrypto> makeCrypto();

} // namespace myytm::platform
