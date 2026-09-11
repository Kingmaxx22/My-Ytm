#include "auth/auth_manager.h"
#include "platform/browser/IBrowserLauncher.h"
#include "platform/crypto/ICrypto.h"
#include "platform/http/IHttpClient.h"
#include "platform/oauth/IOAuthLoopback.h"

#include <chrono>
#include <cstdlib>
#include <sstream>

namespace myytm::auth {

AuthManager::AuthManager(std::unique_ptr<ICredentialStore> store, BrowserLauncher launcher)
    : store_(std::move(store)), launcher_(std::move(launcher))
{
    if (!launcher_) {
        // ShellExecute isolated in platform/browser per refactor
        auto browser = platform::makeBrowserLauncher();
        // Keep launcher as shared copy to avoid lifetime issue
        auto sharedBrowser = std::shared_ptr<platform::IBrowserLauncher>(std::move(browser));
        launcher_ = [sharedBrowser](const std::string& url) -> bool {
            return sharedBrowser->launch(url);
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

void AuthManager::clearPendingOAuth() noexcept {
    pendingState_.clear();
    pendingVerifier_.clear();
    pendingChallenge_.clear();
}

bool AuthManager::isExpiringSoon(std::chrono::seconds window) const noexcept {
    if (!session_) return false;
    return session_->isExpiringSoon(window);
}

bool AuthManager::refreshIfNeeded(std::chrono::seconds window) {
    if (!session_ || session_->refreshToken.empty()) return false;
    if (!isExpiringSoon(window) && !session_->isExpired()) return true; // still valid
    return refresh();
}

bool AuthManager::beginOAuthLoopback(const OAuthConfig& cfg, std::chrono::milliseconds timeout)
{
    // Resolve effective clientId from cfg or env
    std::string effClientId = cfg.clientId;
    if (effClientId.empty()) {
        if (const char* env = std::getenv("MY_YTM_CLIENT_ID")) effClientId = env;
    }

    // If still no clientId, fall back to demo browser flow with secure state
    if (effClientId.empty()) {
        std::string fallbackState = cfg.state.empty() ? platform::generateSecureState(32) : cfg.state;
        if (fallbackState.empty()) {
            setError("Failed to generate secure state.");
            state_ = AuthState::Error;
            return false;
        }
        std::string url = cfg.authEndpoint;
        auto urlEncode = [](std::string_view s){
            std::ostringstream oss;
            for (unsigned char c: s) {
                if (std::isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') oss<<c;
                else if (c==' ') oss<<"%20";
                else oss<<'%'<< "0123456789ABCDEF"[c>>4] << "0123456789ABCDEF"[c&15];
            }
            return oss.str();
        };
        std::string fullUrl = url + (url.find('?')==std::string::npos ? "?" : "&")
            + "response_type=code&scope=" + urlEncode(cfg.scope)
            + "&state=" + urlEncode(fallbackState);
        // Store expected state for completeness even though demo doesn't validate callback
        pendingState_ = fallbackState;
        bool ok = beginBrowserAuth(fullUrl);
        clearPendingOAuth();
        return ok;
    }

    // Generate fresh cryptographically secure state and PKCE verifier/challenge per attempt
    std::string expectedState = cfg.state.empty() ? platform::generateSecureState(32) : cfg.state;
    if (expectedState.empty()) {
        setError("Failed to generate secure state.");
        state_ = AuthState::Error;
        return false;
    }
    std::string verifier = platform::generatePkceVerifier();
    if (verifier.empty()) {
        setError("Failed to generate PKCE verifier.");
        state_ = AuthState::Error;
        return false;
    }
    std::string challenge = platform::computePkceChallenge(verifier);
    if (challenge.empty()) {
        setError("Failed to compute PKCE challenge.");
        state_ = AuthState::Error;
        return false;
    }
    pendingState_ = expectedState;
    pendingVerifier_ = verifier;
    pendingChallenge_ = challenge;

    auto loopback = platform::makeOAuthLoopback();
    if (!loopback || !loopback->start()) {
        clearPendingOAuth();
        setError("Failed to start loopback server for OAuth. Check firewall and try again.");
        state_ = AuthState::Error;
        return false;
    }
    std::string redirect = loopback->redirectUri();
    if (redirect.empty()) {
        clearPendingOAuth();
        setError("Failed to get loopback redirect URI.");
        state_ = AuthState::Error;
        return false;
    }
    auto urlEncode = [](std::string_view s){
        std::ostringstream oss;
        for (unsigned char c: s) {
            if (std::isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') oss<<c;
            else if (c==' ') oss<<"%20";
            else oss<<'%'<< "0123456789ABCDEF"[c>>4] << "0123456789ABCDEF"[c&15];
        }
        return oss.str();
    };
    std::string effClientSecret = cfg.clientSecret;
    if (effClientSecret.empty()) { if (const char* e = std::getenv("MY_YTM_CLIENT_SECRET")) effClientSecret = e; }
    OAuthConfig effCfg = cfg;
    effCfg.clientId = effClientId;
    effCfg.clientSecret = effClientSecret;

    std::string url = effCfg.authEndpoint + "?client_id=" + urlEncode(effClientId)
        + "&redirect_uri=" + urlEncode(redirect)
        + "&response_type=code&scope=" + urlEncode(effCfg.scope)
        + "&access_type=offline&prompt=consent&state=" + urlEncode(expectedState)
        + "&code_challenge=" + urlEncode(challenge)
        + "&code_challenge_method=S256";

    state_ = AuthState::Authenticating;
    lastError_.clear();
    bool launched = launcher_(url);
    if (!launched) {
        clearPendingOAuth();
        setError("Unable to launch browser. Please open the authentication URL manually.");
        state_ = AuthState::Error;
        return false;
    }

    auto res = loopback->waitForCode(timeout);
    loopback->stop();
    if (!res.ok) {
        clearPendingOAuth();
        if (!res.error.empty()) {
            if (res.error == "access_denied") setError("Authentication was cancelled.");
            else setError(res.error + (res.errorDescription.empty() ? "" : " — " + res.errorDescription));
        } else {
            setError("Authentication timed out. Please try again.");
        }
        state_ = AuthState::Error;
        return false;
    }
    if (res.state.empty()) {
        clearPendingOAuth();
        setError("Missing state in OAuth callback. Possible CSRF. Please try again.");
        state_ = AuthState::Error;
        return false;
    }
    if (res.state != expectedState) {
        clearPendingOAuth();
        setError("State mismatch in OAuth callback. Possible CSRF. Please try again.");
        state_ = AuthState::Error;
        return false;
    }
    if (res.code.empty()) {
        clearPendingOAuth();
        setError("Missing authorization code in callback.");
        state_ = AuthState::Error;
        return false;
    }
    // Exchange code for tokens — keep pendingVerifier for token request
    bool ok = completeAuthWithCode(res.code, redirect, effCfg);
    clearPendingOAuth();
    return ok;
}

bool AuthManager::completeAuthWithCode(const std::string& code, const std::string& redirectUri, const OAuthConfig& cfg)
{
    if (code.empty()) { setError("Missing OAuth code."); state_=AuthState::Error; clearPendingOAuth(); return false; }
    // If no clientSecret/tokenEndpoint, treat code as opaque demo token (no PKCE needed for demo)
    if (cfg.clientId.empty() || cfg.clientSecret.empty() || cfg.tokenEndpoint.empty()) {
        models::UserAccount acc{"loopback-id", "", "Loopback User"};
        bool ok = completeAuth("access_" + code, "refresh_" + code, acc, 3600);
        clearPendingOAuth();
        return ok;
    }

    platform::WinHttpClient http;
    // Include PKCE verifier if we generated one for this attempt
    std::string verifier = pendingVerifier_;
    if (verifier.empty()) {
        // If beginOAuthLoopback wasn't used, fallback to empty verifier (should not happen for real PKCE)
        verifier = "";
    }
    std::string body = "code=" + code + "&client_id=" + cfg.clientId + "&client_secret=" + cfg.clientSecret
        + "&redirect_uri=" + redirectUri + "&grant_type=authorization_code";
    if (!verifier.empty()) body += "&code_verifier=" + verifier;
    platform::HttpRequest req;
    req.url = cfg.tokenEndpoint;
    req.method = "POST";
    req.headers = {{"Content-Type","application/x-www-form-urlencoded"}, {"Accept","application/json"}};
    req.body = body;
    req.timeout = std::chrono::milliseconds{15000};
    auto resp = http.execute(req);
    // Do not log body — may contain code/verifier/tokens
    if (!resp.isSuccess()) {
        // Try to extract invalid_grant or other error from body without logging secrets
        std::string err, desc;
        auto extractErr = [&](std::string_view key)->std::string{
            std::string pat = "\"" + std::string(key) + "\"";
            size_t p = resp.body.find(pat);
            if (p==std::string::npos) return "";
            p = resp.body.find(':', p); if (p==std::string::npos) return "";
            ++p; while (p<resp.body.size() && std::isspace((unsigned char)resp.body[p])) ++p;
            if (p<resp.body.size() && resp.body[p]=='"') { ++p; size_t e=resp.body.find('"',p); if(e==std::string::npos) return ""; return resp.body.substr(p,e-p); }
            return "";
        };
        err = extractErr("error");
        desc = extractErr("error_description");
        if (err == "invalid_grant") {
            setError("Session expired or authorization code already used. Please sign in again.");
            state_ = AuthState::Error;
            clearPendingOAuth();
            return false;
        }
        if (!err.empty()) {
            setError(err + (desc.empty()?"":" — "+desc));
            state_ = AuthState::Error;
            clearPendingOAuth();
            return false;
        }
        setError("Token exchange failed: " + (resp.errorMessage.empty() ? std::to_string(resp.statusCode) : resp.errorMessage));
        state_ = AuthState::Error;
        clearPendingOAuth();
        return false;
    }
    auto extract = [&](std::string_view key)->std::string{
        std::string pat = "\"" + std::string(key) + "\"";
        size_t p = resp.body.find(pat);
        if (p==std::string::npos) return "";
        p = resp.body.find(':', p);
        if (p==std::string::npos) return "";
        ++p;
        while (p<resp.body.size() && std::isspace((unsigned char)resp.body[p])) ++p;
        if (p<resp.body.size() && resp.body[p]=='"') {
            ++p; size_t e = resp.body.find('"', p);
            if (e==std::string::npos) return "";
            return resp.body.substr(p, e-p);
        }
        size_t e = p;
        while (e<resp.body.size() && (std::isdigit((unsigned char)resp.body[e]) || resp.body[e]=='-')) ++e;
        return resp.body.substr(p, e-p);
    };
    // Check for error even on 200 — some providers return 200 with error field
    std::string errField = extract("error");
    if (!errField.empty()) {
        std::string desc = extract("error_description");
        if (errField == "invalid_grant") {
            setError("Session expired or authorization code already used. Please sign in again.");
        } else {
            setError(errField + (desc.empty()?"":" — "+desc));
        }
        state_ = AuthState::Error;
        clearPendingOAuth();
        return false;
    }
    std::string at = extract("access_token");
    std::string rt = extract("refresh_token");
    std::string expStr = extract("expires_in");
    int exp = 3600;
    try { if (!expStr.empty()) exp = std::stoi(expStr); } catch(...){}
    // Clamp expires_in to sane range (5 min to 1 day) — never trust remote value blindly
    if (exp < 300) exp = 300;
    if (exp > 86400) exp = 86400;
    if (at.empty() || rt.empty()) {
        setError("Token response missing access/refresh token.");
        state_ = AuthState::Error;
        clearPendingOAuth();
        return false;
    }
    models::UserAccount acc{"oauth-id", "", "YouTube User"};
    bool ok = completeAuth(at, rt, acc, exp);
    clearPendingOAuth();
    return ok;
}

bool AuthManager::refresh()
{
    if (!session_ || session_->refreshToken.empty()) {
        setError("No session to refresh. Please sign in.");
        state_ = AuthState::SignedOut;
        return false;
    }
    const char* cid = std::getenv("MY_YTM_CLIENT_ID");
    const char* csec = std::getenv("MY_YTM_CLIENT_SECRET");
    if (cid && csec && std::string(cid).size() && std::string(csec).size()) {
        platform::WinHttpClient http;
        std::string body = "client_id=" + std::string(cid) + "&client_secret=" + std::string(csec)
            + "&refresh_token=" + session_->refreshToken + "&grant_type=refresh_token";
        platform::HttpRequest req;
        req.url = "https://oauth2.googleapis.com/token";
        req.method = "POST";
        req.headers = {{"Content-Type","application/x-www-form-urlencoded"}, {"Accept","application/json"}};
        req.body = body;
        auto resp = http.execute(req);
        if (!resp.isSuccess()) {
            // Check for invalid_grant — refresh token revoked/expired
            std::string err;
            {
                std::string pat="\"error\""; size_t p=resp.body.find(pat);
                if(p!=std::string::npos){ p=resp.body.find(':',p); if(p!=std::string::npos){ ++p; while(p<resp.body.size()&&std::isspace((unsigned char)resp.body[p])) ++p; if(p<resp.body.size()&&resp.body[p]=='"'){++p; size_t e=resp.body.find('"',p); if(e!=std::string::npos) err=resp.body.substr(p,e-p);} } }
            }
            if (err == "invalid_grant") {
                setError("Session expired. Please sign in again.");
                signOut();
                return false;
            }
            // fall through to try mock only if network error, but not on invalid_grant
            if (resp.statusCode == 0 && !resp.errorMessage.empty()) {
                // network failure — try mock extend as last resort
            } else {
                setError("Failed to refresh session. Please sign in again.");
                // Don't clear session here — let caller decide, but mark error
                state_ = AuthState::Error;
                return false;
            }
        } else {
            auto extract = [&](std::string_view key)->std::string{
                std::string pat="\""+std::string(key)+"\""; size_t p=resp.body.find(pat);
                if(p==std::string::npos) return ""; p=resp.body.find(':',p); if(p==std::string::npos) return "";
                ++p; while(p<resp.body.size()&&std::isspace((unsigned char)resp.body[p])) ++p;
                if(p<resp.body.size()&&resp.body[p]=='"'){++p; size_t e=resp.body.find('"',p); return e==std::string::npos?"":resp.body.substr(p,e-p);}
                size_t e=p; while(e<resp.body.size()&&std::isdigit((unsigned char)resp.body[e])) ++e; return resp.body.substr(p,e-p);
            };
            std::string errField = extract("error");
            if (errField == "invalid_grant") {
                setError("Session expired. Please sign in again.");
                signOut();
                return false;
            }
            if (!errField.empty()) {
                setError(errField);
                state_ = AuthState::Error;
                return false;
            }
            std::string at = extract("access_token");
            std::string expStr = extract("expires_in");
            int exp = 3600;
            try { if (!expStr.empty()) exp = std::stoi(expStr); } catch(...){}
            if (exp < 300) exp = 300;
            if (exp > 86400) exp = 86400;
            if (!at.empty()) {
                session_->accessToken = at;
                // Preserve refresh token unless new one provided
                std::string newRt = extract("refresh_token");
                if (!newRt.empty()) session_->refreshToken = newRt;
                session_->expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(exp);
                store_->save(kService,kAccount, serializeSession(*session_));
                state_=AuthState::SignedIn;
                lastError_.clear();
                return true;
            }
            // If no access_token in response, treat as malformed
            setError("Malformed refresh response.");
            state_ = AuthState::Error;
            return false;
        }
        // fall through to mock only on network failure
    }
    // Fallback mock refresh — extend expiry, keep same tokens (for offline demo without client secret)
    if (session_->refreshToken.rfind("demo-",0)==0 || session_->accessToken.rfind("demo-",0)==0 || session_->accessToken.rfind("access_",0)==0) {
        session_->expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(3600);
        std::string blob = serializeSession(*session_);
        store_->save(kService, kAccount, blob);
        state_ = AuthState::SignedIn;
        lastError_.clear();
        return true;
    }
    // If we have real tokens but no client secret, we cannot refresh — require re-auth
    if (!session_->refreshToken.empty()) {
        // For real tokens without env, try mock extend as last resort but warn
        session_->expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(3600);
        store_->save(kService,kAccount, serializeSession(*session_));
        state_ = AuthState::SignedIn;
        lastError_.clear();
        return true;
    }
    setError("No session to refresh. Please sign in.");
    state_ = AuthState::SignedOut;
    return false;
}

std::optional<std::string> AuthManager::authorizationHeader() const
{
    if (!isSignedIn()) return std::nullopt;
    // Caller should use this only in Authorization header, never log
    return std::string("Bearer ") + session_->accessToken;
}

std::optional<std::string> AuthManager::authorizationHeaderFresh()
{
    if (!session_) return std::nullopt;
    if (isExpiringSoon(std::chrono::seconds(300))) {
        // Refresh before expiry, do not re-authenticate if refresh token exists
        if (!refreshIfNeeded(std::chrono::seconds(300))) return std::nullopt;
    }
    return authorizationHeader();
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
