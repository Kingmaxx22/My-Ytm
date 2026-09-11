#include "auth/credential_store.h"
#include "platform/credentials/ICredentialStore.h"

namespace myytm::auth {

SecureCredentialStore::SecureCredentialStore(std::string appName)
    : impl_(platform::makeSecureStore(std::move(appName))) {}

bool SecureCredentialStore::save(const std::string& service, const std::string& account, const std::string& secret) {
    return impl_->save(service, account, secret);
}
std::optional<std::string> SecureCredentialStore::load(const std::string& service, const std::string& account) {
    return impl_->load(service, account);
}
bool SecureCredentialStore::remove(const std::string& service, const std::string& account) {
    return impl_->remove(service, account);
}

MemoryCredentialStore::MemoryCredentialStore()
    : impl_(platform::makeMemoryStore()) {}

bool MemoryCredentialStore::save(const std::string& service, const std::string& account, const std::string& secret) {
    return impl_->save(service, account, secret);
}
std::optional<std::string> MemoryCredentialStore::load(const std::string& service, const std::string& account) {
    return impl_->load(service, account);
}
bool MemoryCredentialStore::remove(const std::string& service, const std::string& account) {
    return impl_->remove(service, account);
}

} // namespace myytm::auth
