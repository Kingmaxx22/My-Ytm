#pragma once

// Thin shim — real HTTP types live in platform/http per refactor.
// YouTube layer re-exports platform types so existing code keeps myytm::youtube::IHttpClient.
#include "platform/http/IHttpClient.h"

#include <optional>
#include <string>

namespace myytm::youtube {

// Aliases to platform types — keeps layered dependency (YouTube -> Platform)
using HttpResponse = platform::HttpResponse;
using HttpRequest = platform::HttpRequest;
using IHttpClient = platform::IHttpClient;

// Deterministic mock for tests / offline demo. No real network.
class MockHttpClient final : public IHttpClient {
public:
    std::optional<HttpResponse> cannedResponse;
    std::string cannedBody;
    HttpResponse execute(const HttpRequest& req) override;
};

// Platform WinHTTP — implementation lives in src/platform/http/WinHttpClient.cpp
// (CryptProtectData/ShellExecute/GetConsoleScreenBufferInfo moved to platform/*)
class WinHttpClient final : public IHttpClient {
public:
    HttpResponse execute(const HttpRequest& req) override;
};

} // namespace myytm::youtube
