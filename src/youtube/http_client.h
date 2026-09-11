#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace myytm::youtube {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string errorMessage; // low-level, not exposed to UI

    [[nodiscard]] bool isSuccess() const noexcept { return statusCode >= 200 && statusCode < 300; }
};

struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds timeout{10000};
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse execute(const HttpRequest& req) = 0;

    HttpResponse get(const std::string& url, std::chrono::milliseconds timeout = std::chrono::milliseconds{10000})
    {
        return execute(HttpRequest{url, "GET", {}, "", timeout});
    }
};

// Deterministic mock for tests / offline demo. No real network.
class MockHttpClient final : public IHttpClient {
public:
    // If set, always return this response (for error-path tests)
    std::optional<HttpResponse> cannedResponse;

    // canned JSON body for search; if empty uses default demo payload
    std::string cannedBody;

    HttpResponse execute(const HttpRequest& req) override;
};

// Windows stub — isolates WinHTTP behind the interface. Returns error until fully wired;
// keeps UI from containing platform code per AGENTS.md.
class WinHttpClient final : public IHttpClient {
public:
    HttpResponse execute(const HttpRequest& req) override;
};

} // namespace myytm::youtube
