#pragma once
#include <chrono>
#include <memory>
#include <string>

namespace myytm::platform {

// Platform abstraction for OAuth loopback redirect — mirrors browser/http/crypto/terminal pattern.
// No Winsock/Win32 headers leaked to callers (src/auth only sees this interface).
class IOAuthLoopback {
public:
    virtual ~IOAuthLoopback() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
    [[nodiscard]] virtual int port() const noexcept = 0;
    [[nodiscard]] virtual std::string redirectUri() const = 0; // http://127.0.0.1:port/callback

    struct Result {
        bool ok = false;
        std::string code;
        std::string state;
        std::string error;
        std::string errorDescription;
    };

    // Blocks waiting for GET /callback?code=...&state=... — sends HTML response then returns.
    virtual Result waitForCode(std::chrono::milliseconds timeout = std::chrono::minutes(5)) = 0;
};

std::unique_ptr<IOAuthLoopback> makeOAuthLoopback();

} // namespace myytm::platform
