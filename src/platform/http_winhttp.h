#pragma once
#include "youtube/http_client.h"

namespace myytm::platform {

// Real WinHTTP implementation — isolated behind youtube::IHttpClient per AGENTS.md Platform layer.
// Handles https, timeouts, headers, status codes. Never logs secrets.
class WinHttpClientImpl final : public youtube::IHttpClient {
public:
    youtube::HttpResponse execute(const youtube::HttpRequest& req) override;
};

} // namespace myytm::platform
