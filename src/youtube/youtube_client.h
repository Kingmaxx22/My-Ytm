#pragma once

#include "models/search_result.h"
#include "youtube/http_client.h"
#include "youtube/innertube.h"
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
        : http_(std::move(http)), baseUrl_(std::move(baseUrl)) { innertubeConfig_.baseUrl = baseUrl_; }

    // For tests / App wiring
    explicit YouTubeClient(std::unique_ptr<IHttpClient> http, std::string baseUrl, bool useMockSearch)
        : http_(std::move(http)), baseUrl_(std::move(baseUrl)), useMockSearch_(useMockSearch) { innertubeConfig_.baseUrl = baseUrl_; }

    struct SearchPage {
        models::SearchResults results;
        std::optional<std::string> continuationToken;
    };

    [[nodiscard]] Result<models::SearchResults> search(std::string_view query, SearchFilter filter = SearchFilter::All);
    [[nodiscard]] Result<SearchPage> searchPage(std::string_view query, std::optional<std::string_view> continuation = std::nullopt, SearchFilter filter = SearchFilter::All);
    [[nodiscard]] Result<models::SearchResults> getLibrary();
    [[nodiscard]] Result<models::SearchResults> getPlaylists();
    [[nodiscard]] Result<models::SearchResults> getHistory();
    [[nodiscard]] Result<SearchPage> getLibraryPage(std::optional<std::string_view> continuation = std::nullopt);
    [[nodiscard]] Result<SearchPage> getPlaylistsPage(std::optional<std::string_view> continuation = std::nullopt);
    [[nodiscard]] Result<SearchPage> getHistoryPage(std::optional<std::string_view> continuation = std::nullopt);

    // Exposed for validation tests — parses untrusted JSON.
    [[nodiscard]] static Result<models::SearchResults> parseSearchResponse(std::string_view body);
    [[nodiscard]] static Result<SearchPage> parseSearchPage(std::string_view body);
    [[nodiscard]] static Result<SearchPage> parseLibraryPage(std::string_view body);
    [[nodiscard]] static Result<SearchPage> parsePlaylistsPage(std::string_view body);
    [[nodiscard]] static Result<SearchPage> parseHistoryPage(std::string_view body);

    IHttpClient* httpClient() const noexcept { return http_.get(); }
    void setAuthHeaderProvider(std::function<std::optional<std::string>()> provider) { authHeaderProvider_ = std::move(provider); }
    void setInnertubeConfig(const InnertubeConfig& cfg) { innertubeConfig_ = cfg; }
    [[nodiscard]] const InnertubeConfig& innertubeConfig() const noexcept { return innertubeConfig_; }

private:
    static std::string urlEncode(std::string_view s);
    static Result<models::SearchResults> fallbackLocalSearch(std::string_view query, SearchFilter filter = SearchFilter::All);
    void attachAuth(HttpRequest& req) const;
    [[nodiscard]] Result<std::string> innertubePost(std::string_view endpoint, const std::string& jsonBody) const;
    [[nodiscard]] static Result<std::string> handleInnertubeResponse(const HttpResponse& resp);
    static std::optional<std::string> extractApiError(std::string_view body);

    std::unique_ptr<IHttpClient> http_;
    std::string baseUrl_;
    bool useMockSearch_ = true; // true: parse MockHttpClient JSON; false: would hit network
    std::function<std::optional<std::string>()> authHeaderProvider_;
    mutable InnertubeConfig innertubeConfig_;
};

} // namespace myytm::youtube
