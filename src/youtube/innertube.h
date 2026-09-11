#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace myytm::youtube {

// InnerTube configuration — API key never logged, client version tracked.
struct InnertubeConfig {
    std::string apiKey; // empty → env MY_YTM_API_KEY or default public key
    std::string clientName = "WEB_REMIX";
    std::string clientVersion = "1.20240702.01.00";
    std::string baseUrl = "https://music.youtube.com";
    std::string hl = "en";
    std::string gl = "US";
    std::string visitorData; // optional, from previous response or config — never logged
};

// Search filter — maps to InnerTube search `params` (base64 protobuf). Only Songs required for this milestone.
enum class SearchFilter { All, Songs, Videos, Albums, Artists, Playlists };
std::optional<std::string> searchFilterParams(SearchFilter filter); // returns nullopt for All
std::string searchFilterLabel(SearchFilter filter);

// Helpers — keep credentials out of logs, reuse ConfigManager integration.
std::string effectiveApiKey(const InnertubeConfig& cfg);
std::string buildInnertubeUrl(const InnertubeConfig& cfg, std::string_view endpoint);
std::string buildInnertubeContextJson(const InnertubeConfig& cfg);
std::string buildSearchBody(const InnertubeConfig& cfg, std::string_view query, std::optional<std::string_view> continuation = std::nullopt, std::optional<SearchFilter> filter = std::nullopt);
std::string buildBrowseBody(const InnertubeConfig& cfg, std::string_view browseId, std::optional<std::string_view> params = std::nullopt, std::optional<std::string_view> continuation = std::nullopt);
std::string buildLibraryBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation = std::nullopt);
std::string buildPlaylistsBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation = std::nullopt);
std::string buildHistoryBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation = std::nullopt);
std::string buildPlayerBody(const InnertubeConfig& cfg, std::string_view videoId, std::optional<std::string_view> playlistId = std::nullopt);

// JSON escaping for request bodies (query, browseId, etc.)
std::string jsonEscape(std::string_view s);
std::optional<std::string> extractVisitorData(std::string_view body);

} // namespace myytm::youtube
