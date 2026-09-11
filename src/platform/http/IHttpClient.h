#pragma once
#include <chrono>
#include <map>
#include <string>

namespace myytm::platform {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string errorMessage;
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
    HttpResponse get(const std::string& url, std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}) {
        return execute(HttpRequest{url, "GET", {}, "", timeout});
    }
};

// Real WinHTTP implementation — lives in WinHttpClient.cpp, isolated per AGENTS.md
class WinHttpClient final : public IHttpClient {
public:
    HttpResponse execute(const HttpRequest& req) override;
};

} // namespace myytm::platform
