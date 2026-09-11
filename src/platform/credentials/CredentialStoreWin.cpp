#include "platform/credentials/ICredentialStore.h"
#include "platform/crypto/ICrypto.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

namespace myytm::platform {

// Forward declare factory from CryptoWin.cpp
std::unique_ptr<ICrypto> makeCrypto();

namespace {

// Helper: file path for service/account under %APPDATA%/MyYtm
std::string filePathFor(const std::string& appName, const std::string& service, const std::string& account) {
    std::filesystem::path base;
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        base = path; CoTaskMemFree(path);
    } else base = std::filesystem::temp_directory_path();
#else
    const char* home = std::getenv("HOME");
    base = home ? home : std::filesystem::temp_directory_path();
#endif
    base /= appName;
    std::string fname = service + "_" + account + ".bin";
    for (char& c : fname) if (c=='/'||c=='\\'||c==':') c='_';
    return (base / fname).string();
}

} // anonymous

class CredentialStoreWin final : public ICredentialStore {
public:
    explicit CredentialStoreWin(std::string appName = "MyYtm", std::unique_ptr<ICrypto> crypto = nullptr)
        : appName_(std::move(appName)), crypto_(crypto ? std::move(crypto) : makeCrypto()) {}

    bool save(const std::string& service, const std::string& account, const std::string& secret) override {
        try {
            std::string path = filePathFor(appName_, service, account);
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            std::string payload = secret;
            if (crypto_) {
                std::string enc;
                if (crypto_->protect(secret, enc)) payload = enc;
                else return false;
            }
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            return out.good();
        } catch (...) { return false; }
    }

    std::optional<std::string> load(const std::string& service, const std::string& account) override {
        try {
            std::string path = filePathFor(appName_, service, account);
            std::ifstream in(path, std::ios::binary);
            if (!in) return std::nullopt;
            std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (data.empty()) return std::nullopt;
            if (crypto_) {
                std::string plain;
                if (crypto_->unprotect(data, plain)) return plain;
                return std::nullopt;
            }
            return data;
        } catch (...) { return std::nullopt; }
    }

    bool remove(const std::string& service, const std::string& account) override {
        try {
            std::string path = filePathFor(appName_, service, account);
            std::error_code ec;
            std::filesystem::remove(path, ec);
            return !ec;
        } catch (...) { return false; }
    }
private:
    std::string appName_;
    std::unique_ptr<ICrypto> crypto_;
};

class MemoryCredentialStore final : public ICredentialStore {
public:
    bool save(const std::string& service, const std::string& account, const std::string& secret) override {
        map_[service + ":" + account] = secret; return true;
    }
    std::optional<std::string> load(const std::string& service, const std::string& account) override {
        auto it = map_.find(service + ":" + account);
        if (it==map_.end()) return std::nullopt;
        return it->second;
    }
    bool remove(const std::string& service, const std::string& account) override {
        return map_.erase(service + ":" + account) > 0;
    }
private:
    std::map<std::string,std::string> map_;
};

// Factories — keep CryptProtectData isolated via ICrypto
std::unique_ptr<ICredentialStore> makeSecureStore(std::string appName) {
    return std::make_unique<CredentialStoreWin>(std::move(appName));
}
std::unique_ptr<ICredentialStore> makeMemoryStore() {
    return std::make_unique<MemoryCredentialStore>();
}

} // namespace myytm::platform
