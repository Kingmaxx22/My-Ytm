#include "youtube/youtube_client.h"

#include "models/album.h"
#include "models/artist.h"
#include "models/playlist.h"
#include "models/song.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace myytm::youtube {

namespace {

// --- Tiny JSON helpers: handle only the schema we control, but validate strictly ---
// Expected: {"results":[{"type":"song","id":"...","title":"...","subtitle":"..."}, ...]}
// Unknown fields ignored. Missing required fields -> skip entry. Malformed JSON -> Parse error.

void skipWs(std::string_view s, size_t& i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

bool expect(std::string_view s, size_t& i, char c)
{
    skipWs(s, i);
    if (i < s.size() && s[i] == c) { ++i; return true; }
    return false;
}

Result<std::string> parseString(std::string_view s, size_t& i)
{
    skipWs(s, i);
    if (i >= s.size() || s[i] != '"') return Result<std::string>::err(Error::parse("Expected string"));
    ++i;
    std::string out;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return Result<std::string>::ok(std::move(out));
        if (c == '\\') {
            if (i >= s.size()) return Result<std::string>::err(Error::parse("Unterminated escape"));
            char e = s[i++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(e); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return Result<std::string>::err(Error::parse("Unterminated string"));
}

Result<std::map<std::string, std::string>> parseObjectStrings(std::string_view s, size_t& i)
{
    std::map<std::string, std::string> obj;
    if (!expect(s, i, '{')) return Result<std::map<std::string, std::string>>::err(Error::parse("Expected {"));
    skipWs(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return Result<std::map<std::string, std::string>>::ok(std::move(obj)); }
    while (true) {
        auto keyRes = parseString(s, i);
        if (keyRes.isErr()) return Result<std::map<std::string, std::string>>::err(keyRes.error());
        if (!expect(s, i, ':')) return Result<std::map<std::string, std::string>>::err(Error::parse("Expected :"));
        auto valRes = parseString(s, i);
        if (valRes.isErr()) return Result<std::map<std::string, std::string>>::err(valRes.error());
        obj.emplace(keyRes.value(), valRes.value());
        skipWs(s, i);
        if (i >= s.size()) return Result<std::map<std::string, std::string>>::err(Error::parse("Unterminated object"));
        if (s[i] == '}') { ++i; break; }
        if (s[i] == ',') { ++i; continue; }
        return Result<std::map<std::string, std::string>>::err(Error::parse("Expected , or }"));
    }
    return Result<std::map<std::string, std::string>>::ok(std::move(obj));
}

std::string toLowerCopy(std::string_view sv)
{
    std::string r(sv);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

bool containsCi(std::string_view h, std::string_view n)
{
    if (n.empty()) return true;
    auto hl = toLowerCopy(h);
    auto nl = toLowerCopy(n);
    return hl.find(nl) != std::string::npos;
}

} // anonymous

std::string YouTubeClient::urlEncode(std::string_view s)
{
    std::ostringstream oss;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') oss << c;
        else if (c == ' ') oss << "%20";
        else oss << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 15];
    }
    return oss.str();
}

Result<models::SearchResults> YouTubeClient::parseSearchResponse(std::string_view body)
{
    if (body.empty()) return Result<models::SearchResults>::err(Error::parse("Empty response"));
    size_t i = 0;
    skipWs(body, i);
    if (!expect(body, i, '{')) return Result<models::SearchResults>::err(Error::parse("Expected JSON object"));

    models::SearchResults out;

    // Find "results" key
    bool foundResults = false;
    // Very small object scan: expect "results": [ ... ]
    while (i < body.size()) {
        skipWs(body, i);
        if (i < body.size() && body[i] == '}') { ++i; break; }
        auto keyRes = parseString(body, i);
        if (keyRes.isErr()) return Result<models::SearchResults>::err(keyRes.error());
        if (!expect(body, i, ':')) return Result<models::SearchResults>::err(Error::parse("Expected : after key"));
        skipWs(body, i);

        if (keyRes.value() == "results") {
            foundResults = true;
            if (!expect(body, i, '[')) return Result<models::SearchResults>::err(Error::parse("Expected [ for results"));
            skipWs(body, i);
            if (i < body.size() && body[i] == ']') { ++i; } else {
                while (true) {
                    auto objRes = parseObjectStrings(body, i);
                    if (objRes.isErr()) return Result<models::SearchResults>::err(objRes.error());
                    auto& m = objRes.value();
                    auto itType = m.find("type");
                    auto itId = m.find("id");
                    auto itTitle = m.find("title");
                    // subtitle optional
                    std::string subtitle;
                    auto itSub = m.find("subtitle");
                    if (itSub != m.end()) subtitle = itSub->second;

                    // Validate required fields; skip invalid entries (robust vs malformed)
                    if (itType == m.end() || itId == m.end() || itTitle == m.end()) {
                        // skip
                    } else if (itType->second.empty() || itId->second.empty() || itTitle->second.empty()) {
                        // skip invalid
                    } else {
                        std::string t = toLowerCopy(itType->second);
                        if (t == "song") out.push_back(models::SearchResult::fromSong({itId->second, itTitle->second, subtitle, "", std::nullopt}));
                        else if (t == "artist") out.push_back(models::SearchResult::fromArtist({itId->second, itTitle->second}));
                        else if (t == "album") out.push_back(models::SearchResult::fromAlbum({itId->second, itTitle->second, subtitle, std::nullopt}));
                        else if (t == "playlist") out.push_back(models::SearchResult::fromPlaylist({itId->second, itTitle->second, subtitle, 0}));
                        else {
                            // unknown type -> keep as generic song-like
                            models::SearchResult r;
                            r.type = models::SearchResultType::Song;
                            r.id = itId->second;
                            r.title = itTitle->second;
                            r.subtitle = subtitle;
                            out.push_back(std::move(r));
                        }
                    }
                    skipWs(body, i);
                    if (i >= body.size()) return Result<models::SearchResults>::err(Error::parse("Unterminated results array"));
                    if (body[i] == ']') { ++i; break; }
                    if (body[i] == ',') { ++i; continue; }
                    return Result<models::SearchResults>::err(Error::parse("Expected , or ] in results"));
                }
            }
        } else {
            // Skip unknown value — expect string or array/object we skip naively
            // For this schema only "results" matters; consume until , or }
            // If it's a string, parse it; otherwise skip to next comma/brace
            if (i < body.size() && body[i] == '"') {
                auto tmp = parseString(body, i);
                if (tmp.isErr()) return Result<models::SearchResults>::err(tmp.error());
            } else if (i < body.size() && body[i] == '[') {
                int depth = 0;
                while (i < body.size()) {
                    if (body[i] == '[') ++depth;
                    else if (body[i] == ']') { --depth; if (depth == 0) { ++i; break; } }
                    ++i;
                }
            } else if (i < body.size() && body[i] == '{') {
                int depth = 0;
                while (i < body.size()) {
                    if (body[i] == '{') ++depth;
                    else if (body[i] == '}') { --depth; if (depth == 0) { ++i; break; } }
                    ++i;
                }
            } else {
                while (i < body.size() && body[i] != ',' && body[i] != '}') ++i;
            }
        }
        skipWs(body, i);
        if (i < body.size() && body[i] == ',') { ++i; continue; }
        if (i < body.size() && body[i] == '}') { ++i; break; }
    }

    if (!foundResults) return Result<models::SearchResults>::err(Error::parse("Missing 'results' field"));
    return Result<models::SearchResults>::ok(std::move(out));
}

Result<models::SearchResults> YouTubeClient::fallbackLocalSearch(std::string_view query)
{
    // Same catalog as SearchScreen fallback — keeps Phase 3 behavior when offline.
    models::SearchResults catalog = {
        models::SearchResult::fromSong({"s1", "Blinding Lights", "The Weeknd", "After Hours", 200}),
        models::SearchResult::fromSong({"s2", "Bohemian Rhapsody", "Queen", "A Night at the Opera", 354}),
        models::SearchResult::fromSong({"s3", "Hotel California", "Eagles", "Hotel California", 390}),
        models::SearchResult::fromSong({"s4", "Shape of You", "Ed Sheeran", " ÷", 233}),
        models::SearchResult::fromArtist({"a1", "The Weeknd"}),
        models::SearchResult::fromArtist({"a2", "Queen"}),
        models::SearchResult::fromAlbum({"al1", "After Hours", "The Weeknd", 2020}),
        models::SearchResult::fromPlaylist({"p1", "Liked Songs", "You", 128}),
    };
    if (query.empty()) return Result<models::SearchResults>::ok(std::move(catalog));
    models::SearchResults out;
    for (auto& r : catalog) if (containsCi(r.title, query) || containsCi(r.subtitle, query) || containsCi(r.typeLabel(), query)) out.push_back(r);
    return Result<models::SearchResults>::ok(std::move(out));
}

Result<models::SearchResults> YouTubeClient::getLibrary()
{
    if (!http_) return Result<models::SearchResults>::err(Error::network("HTTP client not configured"));
    std::string url = baseUrl_ + "/youtubei/v1/library";
    HttpRequest req{url, "GET", {{"Accept","application/json"}}, "", std::chrono::milliseconds{8000}};
    HttpResponse resp = http_->execute(req);
    if (!resp.errorMessage.empty() && resp.statusCode == 0) {
        // Mock: return liked/library payload
        std::string demo = R"json({"results":[
            {"type":"song","id":"ls1","title":"Liked — Blinding Lights","subtitle":"The Weeknd"},
            {"type":"album","id":"la1","title":"After Hours","subtitle":"The Weeknd"},
            {"type":"artist","id":"la2","title":"The Weeknd","subtitle":""},
            {"type":"song","id":"ls2","title":"Save Your Tears","subtitle":"The Weeknd"},
            {"type":"playlist","id":"lp1","title":"Liked Songs","subtitle":"You • 128 tracks"}
        ]})json";
        return parseSearchResponse(demo);
    }
    if (!resp.isSuccess()) return Result<models::SearchResults>::err(Error{ErrorKind::Network, "Failed to load library (" + std::to_string(resp.statusCode) + ")", resp.statusCode});
    return parseSearchResponse(resp.body);
}

Result<models::SearchResults> YouTubeClient::getPlaylists()
{
    if (!http_) return Result<models::SearchResults>::err(Error::network("HTTP client not configured"));
    std::string url = baseUrl_ + "/youtubei/v1/playlists";
    HttpRequest req{url, "GET", {{"Accept","application/json"}}, "", std::chrono::milliseconds{8000}};
    HttpResponse resp = http_->execute(req);
    if (!resp.errorMessage.empty() && resp.statusCode == 0) {
        std::string demo = R"json({"results":[
            {"type":"playlist","id":"p1","title":"Chill Mix","subtitle":"YouTube Music • 50 tracks"},
            {"type":"playlist","id":"p2","title":"Road Trip","subtitle":"You • 34 tracks"},
            {"type":"playlist","id":"p3","title":"Workout","subtitle":"You • 42 tracks"},
            {"type":"playlist","id":"p4","title":"Liked Songs","subtitle":"You • 128 tracks"}
        ]})json";
        return parseSearchResponse(demo);
    }
    if (!resp.isSuccess()) return Result<models::SearchResults>::err(Error{ErrorKind::Network, "Failed to load playlists (" + std::to_string(resp.statusCode) + ")", resp.statusCode});
    return parseSearchResponse(resp.body);
}

Result<models::SearchResults> YouTubeClient::getHistory()
{
    if (!http_) return Result<models::SearchResults>::err(Error::network("HTTP client not configured"));
    std::string url = baseUrl_ + "/youtubei/v1/history";
    HttpRequest req{url, "GET", {{"Accept","application/json"}}, "", std::chrono::milliseconds{8000}};
    HttpResponse resp = http_->execute(req);
    if (!resp.errorMessage.empty() && resp.statusCode == 0) {
        std::string demo = R"json({"results":[
            {"type":"song","id":"h1","title":"Bohemian Rhapsody","subtitle":"Queen — played 2h ago"},
            {"type":"song","id":"h2","title":"Hotel California","subtitle":"Eagles — played 5h ago"},
            {"type":"song","id":"h3","title":"Viva La Vida","subtitle":"Coldplay — played yesterday"},
            {"type":"album","id":"ha1","title":"Parachutes","subtitle":"Coldplay — viewed yesterday"}
        ]})json";
        return parseSearchResponse(demo);
    }
    if (!resp.isSuccess()) return Result<models::SearchResults>::err(Error{ErrorKind::Network, "Failed to load history (" + std::to_string(resp.statusCode) + ")", resp.statusCode});
    return parseSearchResponse(resp.body);
}

Result<models::SearchResults> YouTubeClient::search(std::string_view query)
{
    if (!http_) return Result<models::SearchResults>::err(Error::network("HTTP client not configured"));

    // Keep request construction isolated — UI never builds URLs.
    std::string url = baseUrl_ + "/youtubei/v1/search?query=" + urlEncode(query);

    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers = {{"Accept", "application/json"}};
    req.timeout = std::chrono::milliseconds{8000};

    HttpResponse resp = http_->execute(req);

    // Handle transport / HTTP errors explicitly per AGENTS.md Networking.
    if (!resp.errorMessage.empty() && resp.statusCode == 0) {
        // WinHttp stub path — fall back to local search so UI stays usable offline
        if (useMockSearch_) {
            // Try to parse whatever body we have (Mock returns demo); if that fails, local fallback
            if (!resp.body.empty()) {
                auto parsed = parseSearchResponse(resp.body);
                if (parsed.isOk()) {
                    // Filter by query like a real service would
                    if (query.empty()) return parsed;
                    models::SearchResults filtered;
                    for (auto& r : parsed.value()) if (containsCi(r.title, query) || containsCi(r.subtitle, query)) filtered.push_back(r);
                    return Result<models::SearchResults>::ok(std::move(filtered));
                }
            }
            return fallbackLocalSearch(query);
        }
        return Result<models::SearchResults>::err(Error::network("Unable to connect to YouTube Music. Check your internet connection and try again."));
    }

    if (resp.statusCode == 401 || resp.statusCode == 403) return Result<models::SearchResults>::err(Error::auth());
    if (resp.statusCode == 429) return Result<models::SearchResults>::err(Error::rateLimited());
    if (resp.statusCode >= 500) return Result<models::SearchResults>::err(Error::network("YouTube Music is temporarily unavailable. Please try again later."));
    if (resp.statusCode == 408) return Result<models::SearchResults>::err(Error::timeout());
    if (!resp.isSuccess()) {
        return Result<models::SearchResults>::err(Error{ErrorKind::Network, "Request failed (" + std::to_string(resp.statusCode) + "). Please try again.", resp.statusCode});
    }
    if (resp.body.empty()) return Result<models::SearchResults>::err(Error::parse("Empty response from YouTube Music"));

    auto parsed = parseSearchResponse(resp.body);
    if (parsed.isErr()) return parsed;

    // Demo Mock returns unfiltered; filter here to emulate server search
    if (query.empty()) return parsed;
    models::SearchResults filtered;
    for (auto& r : parsed.value()) if (containsCi(r.title, query) || containsCi(r.subtitle, query) || containsCi(r.typeLabel(), query)) filtered.push_back(r);
    return Result<models::SearchResults>::ok(std::move(filtered));
}

} // namespace myytm::youtube
