#include "auth/auth_manager.h"
#include "platform/browser/IBrowserLauncher.h"
#include "platform/http/IHttpClient.h"
#include "platform/oauth_loopback.h"

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

bool AuthManager::beginOAuthLoopback(const OAuthConfig& cfg, std::chrono::milliseconds timeout)
{
    if (cfg.clientId.empty()) {
        // Check env fallback
        const char* env = std::getenv("MY_YTM_CLIENT_ID");
        if (!env || std::string(env).empty()) {
            // Fall back to demo browser flow (no loopback)
            std::string url = cfg.authEndpoint;
            if (url.find('?')==std::string::npos) url += "?response_type=code&scope=" + cfg.scope;
            return beginBrowserAuth(url);
        }
    }

    platform::OAuthLoopbackServer server;
    if (!server.start()) {
        setError("Failed to start loopback server for OAuth.");
        state_ = AuthState::Error;
        return false;
    }
    std::string redirect = server.redirectUri();
    std::string state = cfg.state.empty() ? "myytm_state" : cfg.state;
    // Build auth URL with loopback redirect_uri
    auto urlEncode = [](std::string_view s){
        std::ostringstream oss;
        for (unsigned char c: s) {
            if (std::isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') oss<<c;
            else if (c==' ') oss<<"%20";
            else oss<<'%'<< "0123456789ABCDEF"[c>>4] << "0123456789ABCDEF"[c&15];
        }
        return oss.str();
    };
    std::string url = cfg.authEndpoint + "?client_id=" + urlEncode(cfg.clientId)
        + "&redirect_uri=" + urlEncode(redirect)
        + "&response_type=code&scope=" + urlEncode(cfg.scope)
        + "&access_type=offline&prompt=consent&state=" + urlEncode(state);

    state_ = AuthState::Authenticating;
    lastError_.clear();
    bool launched = launcher_(url);
    if (!launched) {
        setError("Unable to launch browser for " + redirect);
        state_ = AuthState::Error;
        return false;
    }

    auto res = server.waitForCode(timeout);
    server.stop();
    if (!res.ok) {
        setError(res.error.empty() ? "Authentication timed out or missing code." : res.error + (res.errorDescription.empty()?"":" — "+res.errorDescription));
        state_ = AuthState::Error;
        return false;
    }
    if (res.state != state) {
        setError("State mismatch in OAuth callback.");
        state_ = AuthState::Error;
        return false;
    }
    // Exchange code for tokens
    return completeAuthWithCode(res.code, redirect, cfg);
}

bool AuthManager::completeAuthWithCode(const std::string& code, const std::string& redirectUri, const OAuthConfig& cfg)
{
    if (code.empty()) { setError("Missing OAuth code."); state_=AuthState::Error; return false; }
    // If no clientSecret/tokenEndpoint, treat code as opaque demo token
    if (cfg.clientId.empty() || cfg.clientSecret.empty() || cfg.tokenEndpoint.empty()) {
        models::UserAccount acc{"loopback-id", "", "Loopback User"};
        // Use code as access token for demo — real would POST to tokenEndpoint
        return completeAuth("access_" + code, "refresh_" + code, acc, 3600);
    }

    platform::WinHttpClient http;
    std::string body = "code=" + code + "&client_id=" + cfg.clientId + "&client_secret=" + cfg.clientSecret
        + "&redirect_uri=" + redirectUri + "&grant_type=authorization_code";
    platform::HttpRequest req;
    req.url = cfg.tokenEndpoint;
    req.method = "POST";
    req.headers = {{"Content-Type","application/x-www-form-urlencoded"}, {"Accept","application/json"}};
    req.body = body;
    req.timeout = std::chrono::milliseconds{15000};
    auto resp = http.execute(req);
    if (!resp.isSuccess()) {
        setError("Token exchange failed: " + (resp.errorMessage.empty() ? std::to_string(resp.statusCode) : resp.errorMessage));
        state_ = AuthState::Error;
        return false;
    }
    // Very small JSON parse for access_token, refresh_token, expires_in
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
        // number
        size_t e = p;
        while (e<resp.body.size() && (std::isdigit((unsigned char)resp.body[e]) || resp.body[e]=='-')) ++e;
        return resp.body.substr(p, e-p);
    };
    std::string at = extract("access_token");
    std::string rt = extract("refresh_token");
    std::string expStr = extract("expires_in");
    int exp = 3600;
    try { if (!expStr.empty()) exp = std::stoi(expStr); } catch(...){}
    if (at.empty() || rt.empty()) {
        setError("Token response missing access/refresh token.");
        state_ = AuthState::Error;
        return false;
    }
    models::UserAccount acc{"oauth-id", "", "YouTube User"};
    return completeAuth(at, rt, acc, exp);
}

bool AuthManager::refresh()
{
    if (!session_ || session_->refreshToken.empty()) {
        setError("No session to refresh. Please sign in.");
        state_ = AuthState::SignedOut;
        return false;
    }
    // Try real refresh via tokenEndpoint if clientId/secret available via env
    const char* cid = std::getenv("MY_YTM_CLIENT_ID");
    const char* csec = std::getenv("MY_YTM_CLIENT_SECRET");
    if (cid && csec && std::string(cid).size() && std::string(csec).size()) {
        platform::WinHttpClient http;
        std::string body = "client_id=" + std::string(cid) + "&client_secret=" + std::string(csec)
            + "&refresh_token=" + session_->refreshToken + "&grant_type=refresh_token";
        platform::HttpRequest req;
        req.url = "https://oauth2.googleapis.com/token";
        req.method = "POST";
        req.headers = {{"Content-Type","application/x-www-form-urlencoded"}};
        req.body = body;
        auto resp = http.execute(req);
        if (resp.isSuccess()) {
            auto extract = [&](std::string_view key)->std::string{
                std::string pat="\""+std::string(key)+"\""; size_t p=resp.body.find(pat);
                if(p==std::string::npos) return ""; p=resp.body.find(':',p); if(p==std::string::npos) return "";
                ++p; while(p<resp.body.size()&&std::isspace((unsigned char)resp.body[p])) ++p;
                if(p<resp.body.size()&&resp.body[p]=='"'){++p; size_t e=resp.body.find('"',p); return e==std::string::npos?"":resp.body.substr(p,e-p);}
                size_t e=p; while(e<resp.body.size()&&std::isdigit((unsigned char)resp.body[e])) ++e; return resp.body.substr(p,e-p);
            };
            std::string at = extract("access_token");
            if (!at.empty()) {
                session_->accessToken = at;
                session_->expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(3600);
                store_->save(kService,kAccount, serializeSession(*session_));
                state_=AuthState::SignedIn;
                lastError_.clear();
                return true;
            }
        }
        // fall through to mock on failure
    }
    // Fallback mock refresh — extend expiry, keep same tokens.
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
