#pragma once
#include <memory>
#include <optional>
#include <string>

namespace myytm::platform {

// Platform credential store interface — DPAPI/file isolated per AGENTS.md.
// Never logs secrets. Auth layer depends on this, not the reverse.
class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;
    virtual bool save(const std::string& service, const std::string& account, const std::string& secret) = 0;
    virtual std::optional<std::string> load(const std::string& service, const std::string& account) = 0;
    virtual bool remove(const std::string& service, const std::string& account) = 0;
};

std::unique_ptr<ICredentialStore> makeSecureStore(std::string appName = "MyYtm");
std::unique_ptr<ICredentialStore> makeMemoryStore();

} // namespace myytm::platform
