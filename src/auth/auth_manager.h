#pragma once

#include "auth/auth_state.h"
#include "auth/credential_store.h"
#include "auth/session.h"
#include "models/user_account.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace myytm::auth {

// Browser-based auth. Never prompts for Google password. Tokens kept opaque.
class AuthManager {
public:
    using BrowserLauncher = std::function<bool(const std::string& url)>; // returns true if launched

    explicit AuthManager(std::unique_ptr<ICredentialStore> store,
                         BrowserLauncher launcher = nullptr);

    [[nodiscard]] AuthState state() const noexcept { return state_; }
    [[nodiscard]] const std::optional<Session>& session() const noexcept { return session_; }
    [[nodiscard]] std::string_view lastError() const noexcept { return lastError_; }
    [[nodiscard]] bool isSignedIn() const noexcept { return state_ == AuthState::SignedIn && session_.has_value() && !session_->isExpired(); }

    // Restore from secure storage on startup
    bool restore();

    // Starts browser flow: opens system browser to auth URL, awaits callback.
    // In this phase the callback is simulated / manual (user completes in browser,
    // session is created via completeAuth). No password handling.
    bool beginBrowserAuth(const std::string& authUrl = "https://accounts.google.com/o/oauth2/v2/auth");

    struct OAuthConfig {
        std::string authEndpoint = "https://accounts.google.com/o/oauth2/v2/auth";
        std::string tokenEndpoint = "https://oauth2.googleapis.com/token";
        std::string clientId; // set via MY_YTM_CLIENT_ID env or config
        std::string clientSecret; // never logged
        std::string scope = "https://www.googleapis.com/auth/youtube";
        std::string state; // random
    };

    // Real loopback flow: spins up 127.0.0.1 ephemeral server, opens browser with
    // redirect_uri=http://127.0.0.1:port/callback, waits for ?code=, exchanges via tokenEndpoint
    // Requires OAuthConfig.clientId; if missing, falls back to beginBrowserAuth demo.
    bool beginOAuthLoopback(const OAuthConfig& cfg, std::chrono::milliseconds timeout = std::chrono::minutes(5));

    // Called after browser flow completes (e.g., OAuth redirect with code exchanged server-side).
    // For Phase 5 demo, accepts opaque tokens directly from a trusted local exchange — never user password.
    bool completeAuth(std::string accessToken, std::string refreshToken, models::UserAccount account, int expiresInSecs = 3600);
    // Exchange code for tokens via tokenEndpoint using IHttpClient (platform WinHTTP)
    bool completeAuthWithCode(const std::string& code, const std::string& redirectUri, const OAuthConfig& cfg);

    bool signOut();
    bool refresh(); // uses refreshToken to get new accessToken (mock in Phase 5)

    // Exposed for YouTubeClient to attach Authorization header without exposing token to UI/logs
    [[nodiscard]] std::optional<std::string> authorizationHeader() const;

private:
    static constexpr const char* kService = "MyYtm";
    static constexpr const char* kAccount = "default";

    std::string serializeSession(const Session& s) const;
    std::optional<Session> deserializeSession(const std::string& blob) const;

    void setError(std::string msg);

    AuthState state_ = AuthState::SignedOut;
    std::optional<Session> session_;
    std::string lastError_;
    std::unique_ptr<ICredentialStore> store_;
    BrowserLauncher launcher_;
};

} // namespace myytm::auth
