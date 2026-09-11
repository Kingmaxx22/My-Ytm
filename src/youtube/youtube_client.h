#pragma once

#include "models/search_result.h"
#include "youtube/http_client.h"
#include "youtube/result.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace myytm::youtube {

class YouTubeClient {
public:
    explicit YouTubeClient(std::unique_ptr<IHttpClient> http, std::string baseUrl = "https://music.youtube.com")
        : http_(std::move(http)), baseUrl_(std::move(baseUrl)) {}

    // For tests / App wiring
    explicit YouTubeClient(std::unique_ptr<IHttpClient> http, std::string baseUrl, bool useMockSearch)
        : http_(std::move(http)), baseUrl_(std::move(baseUrl)), useMockSearch_(useMockSearch) {}

    [[nodiscard]] Result<models::SearchResults> search(std::string_view query);
    [[nodiscard]] Result<models::SearchResults> getLibrary();
    [[nodiscard]] Result<models::SearchResults> getPlaylists();
    [[nodiscard]] Result<models::SearchResults> getHistory();

    // Exposed for validation tests — parses untrusted JSON.
    [[nodiscard]] static Result<models::SearchResults> parseSearchResponse(std::string_view body);

    IHttpClient* httpClient() const noexcept { return http_.get(); }
    void setAuthHeaderProvider(std::function<std::optional<std::string>()> provider) { authHeaderProvider_ = std::move(provider); }

private:
    static std::string urlEncode(std::string_view s);
    static Result<models::SearchResults> fallbackLocalSearch(std::string_view query);
    void attachAuth(HttpRequest& req) const;

    std::unique_ptr<IHttpClient> http_;
    std::string baseUrl_;
    bool useMockSearch_ = true; // true: parse MockHttpClient JSON; false: would hit network
    std::function<std::optional<std::string>()> authHeaderProvider_;
};

} // namespace myytm::youtube
