#pragma once

// Compatibility shim — platform implementation lives in src/platform/credentials & crypto.
// CryptProtectData / file logic moved to platform per refactor. This header re-exports
// platform::ICredentialStore for auth layer so existing code keeps myytm::auth::ICredentialStore.
#include "platform/credentials/ICredentialStore.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace myytm::auth {

// Alias — auth depends on platform::ICredentialStore per layered architecture
using ICredentialStore = platform::ICredentialStore;

// Secure store now delegates to platform::makeSecureStore (which uses platform::ICrypto -> CryptProtectData)
class SecureCredentialStore final : public ICredentialStore {
public:
    explicit SecureCredentialStore(std::string appName = "MyYtm");
    bool save(const std::string& service, const std::string& account, const std::string& secret) override;
    std::optional<std::string> load(const std::string& service, const std::string& account) override;
    bool remove(const std::string& service, const std::string& account) override;

private:
    std::unique_ptr<platform::ICredentialStore> impl_;
};

// In-memory store for tests — delegates to platform::makeMemoryStore
class MemoryCredentialStore final : public ICredentialStore {
public:
    MemoryCredentialStore();
    bool save(const std::string& service, const std::string& account, const std::string& secret) override;
    std::optional<std::string> load(const std::string& service, const std::string& account) override;
    bool remove(const std::string& service, const std::string& account) override;

private:
    std::unique_ptr<platform::ICredentialStore> impl_;
};

} // namespace myytm::auth
