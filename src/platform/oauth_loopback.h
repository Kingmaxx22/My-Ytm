#pragma once
#include <chrono>
#include <optional>
#include <string>

namespace myytm::platform {

// Minimal loopback HTTP server for OAuth redirect on 127.0.0.1 with ephemeral port.
// Isolated in platform layer per AGENTS.md. Uses Winsock, no external deps.
class OAuthLoopbackServer {
public:
    OAuthLoopbackServer() = default;
    ~OAuthLoopbackServer();

    OAuthLoopbackServer(const OAuthLoopbackServer&) = delete;
    OAuthLoopbackServer& operator=(const OAuthLoopbackServer&) = delete;

    // Starts listening on 127.0.0.1:0 (ephemeral). Returns true on success.
    bool start();
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return running_; }
    [[nodiscard]] int port() const noexcept { return port_; }
    [[nodiscard]] std::string redirectUri() const; // http://127.0.0.1:port/callback

    struct Result {
        bool ok = false;
        std::string code;
        std::string state;
        std::string error;
        std::string errorDescription;
    };

    // Blocks waiting for a single GET /callback?code=...&state=... request.
    // Sends a minimal HTML response ("You can close this window...") then returns.
    // Timeout default 5 minutes. Returns {ok=false} on timeout/error.
    Result waitForCode(std::chrono::milliseconds timeout = std::chrono::minutes(5));

private:
    bool running_ = false;
    int port_ = 0;
#ifdef _WIN32
    void* listenSock_ = nullptr; // SOCKET
    bool wsaInit_ = false;
#endif
    std::string lastError_;
};

} // namespace myytm::platform
