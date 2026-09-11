#pragma once

#include <map>
#include <optional>
#include <string>

namespace myytm::auth {

// Interface so platform code stays isolated per AGENTS.md.
// Implementations must never log tokens or write them to unprotected files.
class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;
    virtual bool save(const std::string& service, const std::string& account, const std::string& secret) = 0;
    virtual std::optional<std::string> load(const std::string& service, const std::string& account) = 0;
    virtual bool remove(const std::string& service, const std::string& account) = 0;
};

// Windows DPAPI-backed store: encrypts with user scope (CryptProtectData).
// Falls back to encrypted file under %APPDATA% if DPAPI unavailable.
// Non-Windows: file store with restrictive permissions (demo-only).
class SecureCredentialStore final : public ICredentialStore {
public:
    explicit SecureCredentialStore(std::string appName = "MyYtm");
    bool save(const std::string& service, const std::string& account, const std::string& secret) override;
    std::optional<std::string> load(const std::string& service, const std::string& account) override;
    bool remove(const std::string& service, const std::string& account) override;

private:
    std::string appName_;
    std::string filePathFor(const std::string& service, const std::string& account) const;

#ifdef _WIN32
    bool dpapiProtect(const std::string& plain, std::string& out) const;
    bool dpapiUnprotect(const std::string& enc, std::string& out) const;
#endif
};

// In-memory store for tests — never touches disk.
class MemoryCredentialStore final : public ICredentialStore {
public:
    bool save(const std::string& service, const std::string& account, const std::string& secret) override;
    std::optional<std::string> load(const std::string& service, const std::string& account) override;
    bool remove(const std::string& service, const std::string& account) override;

private:
    std::string keyFor(const std::string& service, const std::string& account) const;
    std::map<std::string, std::string> map_;
};

} // namespace myytm::auth
