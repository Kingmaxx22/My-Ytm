#include "auth/credential_store.h"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#include <shlobj.h>
#endif

namespace myytm::auth {

SecureCredentialStore::SecureCredentialStore(std::string appName) : appName_(std::move(appName)) {}

std::string SecureCredentialStore::filePathFor(const std::string& service, const std::string& account) const
{
    std::filesystem::path base;
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        base = path;
        CoTaskMemFree(path);
    } else {
        base = std::filesystem::temp_directory_path();
    }
#else
    const char* home = std::getenv("HOME");
    base = home ? home : std::filesystem::temp_directory_path();
#endif
    base /= appName_;
    // Do not embed raw account/service in log messages; path only for internal use
    std::string fname = service + "_" + account + ".bin";
    // Sanitize
    for (char& c : fname) if (c == '/' || c == '\\' || c == ':') c = '_';
    return (base / fname).string();
}

#ifdef _WIN32
bool SecureCredentialStore::dpapiProtect(const std::string& plain, std::string& out) const
{
    DATA_BLOB in{}, outBlob{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());
    if (!CryptProtectData(&in, L"MyYtm", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) return false;
    out.assign(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

bool SecureCredentialStore::dpapiUnprotect(const std::string& enc, std::string& out) const
{
    DATA_BLOB in{}, outBlob{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(enc.data()));
    in.cbData = static_cast<DWORD>(enc.size());
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) return false;
    out.assign(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}
#endif

bool SecureCredentialStore::save(const std::string& service, const std::string& account, const std::string& secret)
{
    try {
        std::string path = filePathFor(service, account);
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());

        std::string payload = secret;
#ifdef _WIN32
        std::string enc;
        if (dpapiProtect(secret, enc)) payload = enc;
        else return false;
#endif
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        return out.good();
    } catch (...) {
        return false;
    }
}

std::optional<std::string> SecureCredentialStore::load(const std::string& service, const std::string& account)
{
    try {
        std::string path = filePathFor(service, account);
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.empty()) return std::nullopt;
#ifdef _WIN32
        std::string plain;
        if (dpapiUnprotect(data, plain)) return plain;
        return std::nullopt;
#else
        return data;
#endif
    } catch (...) {
        return std::nullopt;
    }
}

bool SecureCredentialStore::remove(const std::string& service, const std::string& account)
{
    try {
        std::string path = filePathFor(service, account);
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return !ec;
    } catch (...) {
        return false;
    }
}

// Memory store
std::string MemoryCredentialStore::keyFor(const std::string& service, const std::string& account) const
{
    return service + ":" + account;
}

bool MemoryCredentialStore::save(const std::string& service, const std::string& account, const std::string& secret)
{
    map_[keyFor(service, account)] = secret;
    return true;
}

std::optional<std::string> MemoryCredentialStore::load(const std::string& service, const std::string& account)
{
    auto it = map_.find(keyFor(service, account));
    if (it == map_.end()) return std::nullopt;
    return it->second;
}

bool MemoryCredentialStore::remove(const std::string& service, const std::string& account)
{
    return map_.erase(keyFor(service, account)) > 0;
}

} // namespace myytm::auth
