#include "auth/auth_manager.h"

#include <chrono>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace myytm::auth {

AuthManager::AuthManager(std::unique_ptr<ICredentialStore> store, BrowserLauncher launcher)
    : store_(std::move(store)), launcher_(std::move(launcher))
{
    if (!launcher_) {
        launcher_ = [](const std::string& url) -> bool {
#ifdef _WIN32
            HINSTANCE r = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return reinterpret_cast<intptr_t>(r) > 32;
#else
            (void)url;
            return false;
#endif
        };
    }
}

bool AuthManager::restore()
{
    auto blob = store_->load(kService, kAccount);
    if (!blob) {
        state_ = AuthState::SignedOut;
        session_.reset();
        return false;
    }
    auto sess = deserializeSession(*blob);
    if (!sess || sess->isExpired() || !sess->hasTokens()) {
        store_->remove(kService, kAccount);
        state_ = AuthState::SignedOut;
        session_.reset();
        return false;
    }
    session_ = std::move(sess);
    state_ = AuthState::SignedIn;
    lastError_.clear();
    return true;
}

bool AuthManager::beginBrowserAuth(const std::string& authUrl)
{
    // Never collect password — just launch browser per AGENTS.md
    state_ = AuthState::Authenticating;
    lastError_.clear();
    bool ok = launcher_(authUrl);
    if (!ok) {
        setError("Unable to launch browser. Please open the authentication URL manually.");
        state_ = AuthState::Error;
        return false;
    }
    // In real flow we'd spin a local loopback server to capture redirect.
    // Phase 5 leaves session pending until completeAuth() is called.
    return true;
}

bool AuthManager::completeAuth(std::string accessToken, std::string refreshToken, models::UserAccount account, int expiresInSecs)
{
    if (accessToken.empty() || refreshToken.empty()) {
        setError("Authentication failed. Missing credentials.");
        state_ = AuthState::Error;
        return false;
    }
    if (!account.isValid()) {
        setError("Authentication failed. Invalid account.");
        state_ = AuthState::Error;
        return false;
    }

    Session s;
    s.account = std::move(account);
    s.accessToken = std::move(accessToken);
    s.refreshToken = std::move(refreshToken);
    s.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(expiresInSecs);

    std::string blob = serializeSession(s);
    // Do not log blob — contains tokens
    if (!store_->save(kService, kAccount, blob)) {
        setError("Failed to persist session securely.");
        state_ = AuthState::Error;
        return false;
    }

    session_ = std::move(s);
    state_ = AuthState::SignedIn;
    lastError_.clear();
    return true;
}

bool AuthManager::signOut()
{
    if (session_) session_->clearSecrets();
    session_.reset();
    store_->remove(kService, kAccount);
    state_ = AuthState::SignedOut;
    lastError_.clear();
    return true;
}

bool AuthManager::refresh()
{
    if (!session_ || session_->refreshToken.empty()) {
        setError("No session to refresh. Please sign in.");
        state_ = AuthState::SignedOut;
        return false;
    }
    // Phase 5 mock refresh — extend expiry, keep same tokens.
    // Real implementation would POST to token endpoint via IHttpClient.
    session_->expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(3600);
    std::string blob = serializeSession(*session_);
    store_->save(kService, kAccount, blob);
    state_ = AuthState::SignedIn;
    lastError_.clear();
    return true;
}

std::optional<std::string> AuthManager::authorizationHeader() const
{
    if (!isSignedIn()) return std::nullopt;
    // Caller should use this only in Authorization header, never log
    return std::string("Bearer ") + session_->accessToken;
}

std::string AuthManager::serializeSession(const Session& s) const
{
    // Simple line-based serialization — service-specific, encrypted at rest via DPAPI
    // Do not add logging here.
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(s.expiresAt.time_since_epoch()).count();
    std::ostringstream oss;
    oss << s.account.id << "\n" << s.account.email << "\n" << s.account.displayName << "\n"
        << s.accessToken << "\n" << s.refreshToken << "\n" << secs << "\n";
    return oss.str();
}

std::optional<Session> AuthManager::deserializeSession(const std::string& blob) const
{
    std::istringstream iss(blob);
    std::string id, email, displayName, access, refresh, expStr;
    if (!std::getline(iss, id)) return std::nullopt;
    if (!std::getline(iss, email)) return std::nullopt;
    if (!std::getline(iss, displayName)) return std::nullopt;
    if (!std::getline(iss, access)) return std::nullopt;
    if (!std::getline(iss, refresh)) return std::nullopt;
    if (!std::getline(iss, expStr)) return std::nullopt;
    Session s;
    s.account.id = id;
    s.account.email = email;
    s.account.displayName = displayName;
    s.accessToken = access;
    s.refreshToken = refresh;
    try {
        long long secs = std::stoll(expStr);
        s.expiresAt = std::chrono::system_clock::time_point(std::chrono::seconds(secs));
    } catch (...) {
        return std::nullopt;
    }
    return s;
}

void AuthManager::setError(std::string msg)
{
    lastError_ = std::move(msg);
}

} // namespace myytm::auth
