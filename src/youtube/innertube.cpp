#include "youtube/innertube.h"
#include "config/config.h"

#include <cstdlib>
#include <sstream>

namespace myytm::youtube {

std::string effectiveApiKey(const InnertubeConfig& cfg) {
    if (!cfg.apiKey.empty()) return cfg.apiKey;
    if (const char* env = std::getenv("MY_YTM_API_KEY")) {
        if (env[0] != '\0') return std::string(env);
    }
    if (const char* env2 = std::getenv("YOUTUBE_API_KEY")) {
        if (env2[0] != '\0') return std::string(env2);
    }
    // Public WEB_REMIX key — not secret, used by YouTube Music web client. Never log.
    return "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8";
}

std::string buildInnertubeUrl(const InnertubeConfig& cfg, std::string_view endpoint) {
    std::string base = cfg.baseUrl;
    if (base.empty()) base = "https://music.youtube.com";
    // Ensure no trailing slash duplication
    if (!base.empty() && base.back() == '/') base.pop_back();
    std::string url = base + "/youtubei/v1/" + std::string(endpoint);
    std::string key = effectiveApiKey(cfg);
    if (!key.empty()) {
        url += "?key=" + key;
        url += "&prettyPrint=false";
    } else {
        url += "?prettyPrint=false";
    }
    return url;
}

std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

std::string buildInnertubeContextJson(const InnertubeConfig& cfg) {
    std::ostringstream oss;
    oss << "\"context\":{\"client\":{";
    oss << "\"clientName\":\"" << jsonEscape(cfg.clientName) << "\",";
    oss << "\"clientVersion\":\"" << jsonEscape(cfg.clientVersion) << "\",";
    oss << "\"hl\":\"" << jsonEscape(cfg.hl) << "\",";
    oss << "\"gl\":\"" << jsonEscape(cfg.gl) << "\"";
    oss << "},\"user\":{\"lockedSafetyMode\":false}}";
    return oss.str();
}

std::string buildSearchBody(const InnertubeConfig& cfg, std::string_view query, std::optional<std::string_view> continuation) {
    std::ostringstream oss;
    oss << "{" << buildInnertubeContextJson(cfg) << ",";
    if (continuation && !continuation->empty()) {
        oss << "\"continuation\":\"" << jsonEscape(*continuation) << "\"";
    } else {
        oss << "\"query\":\"" << jsonEscape(query) << "\"";
    }
    oss << "}";
    return oss.str();
}

std::string buildBrowseBody(const InnertubeConfig& cfg, std::string_view browseId, std::optional<std::string_view> params, std::optional<std::string_view> continuation) {
    std::ostringstream oss;
    oss << "{" << buildInnertubeContextJson(cfg) << ",";
    oss << "\"browseId\":\"" << jsonEscape(browseId) << "\"";
    if (params && !params->empty()) oss << ",\"params\":\"" << jsonEscape(*params) << "\"";
    if (continuation && !continuation->empty()) oss << ",\"continuation\":\"" << jsonEscape(*continuation) << "\"";
    oss << "}";
    return oss.str();
}

std::string buildLibraryBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation) {
    // FEmusic_library_corpus or FEmusic_liked etc. Use generic library browseId that works for WEB_REMIX
    return buildBrowseBody(cfg, "FEmusic_library_corpus", std::nullopt, continuation);
}
std::string buildPlaylistsBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation) {
    return buildBrowseBody(cfg, "FEmusic_liked_playlists", std::nullopt, continuation);
}
std::string buildHistoryBrowseBody(const InnertubeConfig& cfg, std::optional<std::string_view> continuation) {
    return buildBrowseBody(cfg, "FEmusic_history", std::nullopt, continuation);
}

} // namespace myytm::youtube
