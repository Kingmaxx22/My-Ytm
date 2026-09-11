#include "youtube/youtube_client.h"
#include "youtube/json.h"

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

// --- InnerTube helpers (nested renderers) ---
const json::JsonValue* getNested(const json::JsonValue* root, std::initializer_list<std::string> path) {
    const json::JsonValue* cur = root;
    for (auto& key : path) {
        if (!cur || !cur->isObject()) return nullptr;
        cur = cur->get(key);
        if (!cur) return nullptr;
    }
    return cur;
}

std::optional<std::string> findFirstString(const json::JsonValue& node, const std::string& key) {
    if (node.isObject()) {
        if (auto v = node.get(key); v && v->isString()) return v->asString();
        for (auto& kv : node.asObject()) {
            if (auto r = findFirstString(kv.second, key)) return r;
        }
    } else if (node.isArray()) {
        for (auto& e : node.asArray()) {
            if (auto r = findFirstString(e, key)) return r;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extractContinuationToken(const json::JsonValue& root) {
    // Search for continuations -> nextContinuationData/nextRadioContinuationData -> continuation
    std::vector<const json::JsonValue*> contNodes;
    json::collectRenderers(root, "continuations", contNodes);
    for (auto* n : contNodes) {
        if (!n->isArray()) continue;
        for (auto& c : n->asArray()) {
            if (auto* next = c.get("nextContinuationData")) {
                if (auto tok = json::getString(*next, "continuation")) return tok;
            }
            if (auto* nextRadio = c.get("nextRadioContinuationData")) {
                if (auto tok = json::getString(*nextRadio, "continuation")) return tok;
            }
            // Sometimes continuation is nested deeper
            if (auto tok = findFirstString(c, "continuation")) return tok;
        }
    }
    // Fallback: search anywhere for nextContinuationData
    if (auto* next = json::findRecursive(root, "nextContinuationData")) {
        if (auto tok = json::getString(*next, "continuation")) return tok;
    }
    return std::nullopt;
}

std::optional<models::SearchResult> tryParseResponsiveItem(const json::JsonValue& renderer) {
    // renderer is the object under musicResponsiveListItemRenderer
    // Extract flexColumns
    const json::JsonValue* flexColumns = renderer.get("flexColumns");
    if (!flexColumns || !flexColumns->isArray() || flexColumns->asArray().empty()) return std::nullopt;

    // Title from flexColumns[0]
    std::string title;
    if (auto* c0 = flexColumns->at(0)) {
        if (auto* fc = c0->get("musicResponsiveListItemFlexColumnRenderer")) {
            if (auto* text = fc->get("text")) title = json::extractRunsText(text);
        }
    }
    if (title.empty()) return std::nullopt;

    // Subtitle/artist/album from flexColumns[1] if exists
    std::string subtitle;
    if (flexColumns->asArray().size() > 1) {
        if (auto* c1 = flexColumns->at(1)) {
            if (auto* fc = c1->get("musicResponsiveListItemFlexColumnRenderer")) {
                if (auto* text = fc->get("text")) subtitle = json::extractRunsText(text);
            }
        }
    }
    // Duration from fixedColumns[0] or flexColumns[2]
    std::string durationStr;
    if (auto* fixed = renderer.get("fixedColumns")) {
        if (fixed->isArray() && !fixed->asArray().empty()) {
            if (auto* fc = fixed->at(0)) {
                if (auto* r = fc->get("musicResponsiveListItemFixedColumnRenderer")) {
                    if (auto* text = r->get("text")) durationStr = json::extractRunsText(text);
                }
            }
        }
    }
    if (durationStr.empty() && flexColumns->asArray().size() > 2) {
        if (auto* c2 = flexColumns->at(2)) {
            if (auto* fc = c2->get("musicResponsiveListItemFlexColumnRenderer")) {
                if (auto* text = fc->get("text")) {
                    std::string cand = json::extractRunsText(text);
                    if (cand.find(':') != std::string::npos) durationStr = cand;
                }
            }
        }
    }
    std::optional<int> duration;
    if (!durationStr.empty()) duration = json::parseDuration(durationStr);

    // Thumbnail
    std::optional<std::string> thumb;
    if (auto* th = renderer.get("thumbnail")) thumb = json::extractThumbnail(th);

    // ID and type
    std::string id;
    models::SearchResultType type = models::SearchResultType::Song;
    // Search for videoId/browseId anywhere in renderer
    if (auto vid = findFirstString(renderer, "videoId")) {
        id = *vid;
        type = models::SearchResultType::Song;
    } else if (auto bid = findFirstString(renderer, "browseId")) {
        id = *bid;
        if (id.rfind("UC",0)==0) type = models::SearchResultType::Artist;
        else if (id.rfind("MPRE",0)==0) type = models::SearchResultType::Album;
        else if (id.rfind("VL",0)==0 || id.rfind("PL",0)==0) type = models::SearchResultType::Playlist;
        else type = models::SearchResultType::Album; // default for browse
    } else if (auto pid = findFirstString(renderer, "playlistId")) {
        id = *pid;
        type = models::SearchResultType::Playlist;
    }
    if (id.empty()) return std::nullopt;

    models::SearchResult res;
    res.type = type;
    res.id = id;
    res.title = title;
    res.subtitle = subtitle;
    // Payload
    if (type == models::SearchResultType::Song) {
        models::Song s; s.id=id; s.title=title; s.artist=subtitle; s.durationSeconds=duration; s.thumbnailUrl=thumb;
        res.payload = s;
    } else if (type == models::SearchResultType::Artist) {
        models::Artist a; a.id=id; a.name=title; a.thumbnailUrl=thumb;
        res.payload = a;
    } else if (type == models::SearchResultType::Album) {
        models::Album a; a.id=id; a.title=title; a.artist=subtitle; a.thumbnailUrl=thumb;
        res.payload = a;
    } else if (type == models::SearchResultType::Playlist) {
        models::Playlist p; p.id=id; p.title=title; p.author=subtitle;
        res.payload = p;
    }
    return res;
}

std::optional<models::SearchResult> tryParseTwoRowItem(const json::JsonValue& renderer) {
    // musicTwoRowItemRenderer: title, subtitle, navigationEndpoint, thumbnailRenderer
    std::string title;
    if (auto* t = renderer.get("title")) title = json::extractRunsText(t);
    if (title.empty()) return std::nullopt;
    std::string subtitle;
    if (auto* sub = renderer.get("subtitle")) subtitle = json::extractRunsText(sub);

    std::optional<std::string> thumb;
    if (auto* th = renderer.get("thumbnailRenderer")) thumb = json::extractThumbnail(th);
    else if (auto* th2 = renderer.get("thumbnail")) thumb = json::extractThumbnail(th2);

    std::string id;
    models::SearchResultType type = models::SearchResultType::Playlist;
    if (auto vid = findFirstString(renderer, "videoId")) { id=*vid; type=models::SearchResultType::Song; }
    else if (auto bid = findFirstString(renderer, "browseId")) {
        id=*bid;
        if (id.rfind("UC",0)==0) type=models::SearchResultType::Artist;
        else if (id.rfind("MPRE",0)==0) type=models::SearchResultType::Album;
        else type=models::SearchResultType::Playlist;
    } else if (auto pid = findFirstString(renderer, "playlistId")) { id=*pid; type=models::SearchResultType::Playlist; }
    if (id.empty()) return std::nullopt;

    models::SearchResult res; res.type=type; res.id=id; res.title=title; res.subtitle=subtitle;
    if (type==models::SearchResultType::Song) res.payload=models::Song{id,title,subtitle,"",std::nullopt,thumb};
    else if (type==models::SearchResultType::Artist) res.payload=models::Artist{id,title,thumb};
    else if (type==models::SearchResultType::Album) res.payload=models::Album{id,title,subtitle,std::nullopt,thumb};
    else res.payload=models::Playlist{id,title,subtitle,0};
    return res;
}

YouTubeClient::SearchPage parseInnerTubePage(const json::JsonValue& root) {
    YouTubeClient::SearchPage page;
    std::vector<const json::JsonValue*> responsive, twoRow;
    json::collectRenderers(root, "musicResponsiveListItemRenderer", responsive);
    json::collectRenderers(root, "musicTwoRowItemRenderer", twoRow);
    for (auto* r : responsive) {
        if (auto sr = tryParseResponsiveItem(*r)) page.results.push_back(std::move(*sr));
    }
    for (auto* r : twoRow) {
        if (auto sr = tryParseTwoRowItem(*r)) page.results.push_back(std::move(*sr));
    }
    if (auto tok = extractContinuationToken(root)) page.continuationToken = *tok;
    return page;
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

Result<models::SearchResults> YouTubeClient::fallbackLocalSearch(std::string_view query, SearchFilter filter)
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
    auto typeMatches = [&](const models::SearchResult& r){
        if (filter == SearchFilter::All) return true;
        if (filter == SearchFilter::Songs) return r.type == models::SearchResultType::Song;
        if (filter == SearchFilter::Videos) return r.type == models::SearchResultType::Song; // Videos map to Song type in mock
        if (filter == SearchFilter::Albums) return r.type == models::SearchResultType::Album;
        if (filter == SearchFilter::Artists) return r.type == models::SearchResultType::Artist;
        if (filter == SearchFilter::Playlists) return r.type == models::SearchResultType::Playlist;
        return true;
    };
    if (query.empty()) {
        models::SearchResults out;
        for (auto& r : catalog) if (typeMatches(r)) out.push_back(r);
        return Result<models::SearchResults>::ok(std::move(out));
    }
    models::SearchResults out;
    for (auto& r : catalog) if (typeMatches(r) && (containsCi(r.title, query) || containsCi(r.subtitle, query) || containsCi(r.typeLabel(), query))) out.push_back(r);
    return Result<models::SearchResults>::ok(std::move(out));
}

Result<std::string> YouTubeClient::innertubePost(std::string_view endpoint, const std::string& jsonBody) const {
    if (!http_) return Result<std::string>::err(Error::network("HTTP client not configured"));
    std::string url = buildInnertubeUrl(innertubeConfig_, endpoint);
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.headers = {{"Content-Type", "application/json"}, {"Accept", "application/json"}, {"Origin", "https://music.youtube.com"}, {"Referer", "https://music.youtube.com/"}};
    // X-Goog-Api-Key header for compatibility — keep URL key as well, never log
    if (auto key = effectiveApiKey(innertubeConfig_); !key.empty()) req.headers["X-Goog-Api-Key"] = key;
    if (!innertubeConfig_.visitorData.empty()) req.headers["X-Goog-Visitor-Id"] = innertubeConfig_.visitorData;
    req.body = jsonBody;
    req.timeout = std::chrono::milliseconds{8000};
    attachAuth(req);
    HttpResponse resp = http_->execute(req);
    auto result = handleInnertubeResponse(resp);
    // Capture visitorData if present for next requests — never log value
    if (result.isOk()) {
        if (auto vd = extractVisitorData(result.value())) {
            innertubeConfig_.visitorData = *vd;
        } else if (auto vd2 = extractVisitorData(resp.body)) {
            innertubeConfig_.visitorData = *vd2;
        }
    } else {
        // Even on error, try to capture visitorData from body for future requests
        if (auto vd = extractVisitorData(resp.body)) innertubeConfig_.visitorData = *vd;
    }
    return result;
}

Result<std::string> YouTubeClient::handleInnertubeResponse(const HttpResponse& resp) {
    if (!resp.errorMessage.empty() && resp.statusCode == 0) {
        return Result<std::string>::err(Error::network("Unable to connect to YouTube Music. Check your internet connection and try again."));
    }
    if (resp.statusCode == 401 || resp.statusCode == 403) return Result<std::string>::err(Error::auth());
    if (resp.statusCode == 429) return Result<std::string>::err(Error::rateLimited());
    if (resp.statusCode >= 500) return Result<std::string>::err(Error::network("YouTube Music is temporarily unavailable. Please try again later."));
    if (resp.statusCode == 408) return Result<std::string>::err(Error::timeout());
    if (!resp.isSuccess()) {
        if (auto apiErr = extractApiError(resp.body)) return Result<std::string>::err(Error::parse(*apiErr));
        return Result<std::string>::err(Error{ErrorKind::Network, "Request failed (" + std::to_string(resp.statusCode) + "). Please try again.", resp.statusCode});
    }
    if (resp.body.empty()) return Result<std::string>::err(Error::parse("Empty response from YouTube Music"));
    if (auto apiErr = extractApiError(resp.body)) return Result<std::string>::err(Error::parse(*apiErr));
    return Result<std::string>::ok(resp.body);
}

std::optional<std::string> YouTubeClient::extractApiError(std::string_view body) {
    // InnerTube error often: {"error":{"code":403,"message":"...","status":"PERMISSION_DENIED"}}
    // or {"error":{"message":"...","errors":[{"reason":"..."}]}}
    // We look for "error" object with "message" field — lightweight scan, never assumes valid JSON.
    size_t pos = body.find("\"error\"");
    if (pos == std::string_view::npos) return std::nullopt;
    // Find "message" after error
    size_t msgPos = body.find("\"message\"", pos);
    if (msgPos == std::string_view::npos) return std::nullopt;
    size_t colon = body.find(':', msgPos);
    if (colon == std::string_view::npos) return std::nullopt;
    size_t q1 = body.find('"', colon);
    if (q1 == std::string_view::npos) return std::nullopt;
    size_t q2 = body.find('"', q1+1);
    if (q2 == std::string_view::npos) return std::nullopt;
    std::string msg(body.substr(q1+1, q2-q1-1));
    if (msg.empty()) return std::nullopt;
    // Distinguish auth vs generic
    if (msg.find("API key") != std::string::npos) msg = "Invalid API key. Check configuration.";
    return msg;
}

Result<YouTubeClient::SearchPage> YouTubeClient::searchPage(std::string_view query, std::optional<std::string_view> continuation, SearchFilter filter) {
    std::string body = buildSearchBody(innertubeConfig_, query, continuation, filter == SearchFilter::All ? std::nullopt : std::optional<SearchFilter>(filter));
    auto res = innertubePost("search", body);
    if (res.isErr()) {
        if (res.error().kind == ErrorKind::Network && res.error().message.find("Unable to connect") != std::string::npos && useMockSearch_) {
            // Offline fallback: try mock local search for first page only
            if (!continuation || continuation->empty()) {
                auto fb = fallbackLocalSearch(query, filter);
                if (fb.isOk()) return Result<SearchPage>::ok(SearchPage{fb.value(), std::nullopt});
            }
        }
        return Result<SearchPage>::err(res.error());
    }
    return parseSearchPage(res.value());
}

Result<YouTubeClient::SearchPage> YouTubeClient::getLibraryPage(std::optional<std::string_view> continuation) {
    std::string body = buildLibraryBrowseBody(innertubeConfig_, continuation);
    auto res = innertubePost("browse", body);
    if (res.isErr()) {
        if (res.error().kind == ErrorKind::Network && useMockSearch_) {
            std::string demo = R"json({"results":[
                {"type":"song","id":"ls1","title":"Liked — Blinding Lights","subtitle":"The Weeknd"},
                {"type":"album","id":"la1","title":"After Hours","subtitle":"The Weeknd"},
                {"type":"artist","id":"la2","title":"The Weeknd","subtitle":""},
                {"type":"song","id":"ls2","title":"Save Your Tears","subtitle":"The Weeknd"},
                {"type":"playlist","id":"lp1","title":"Liked Songs","subtitle":"You • 128 tracks"}
            ]})json";
            auto parsed = parseSearchResponse(demo);
            if (parsed.isOk()) return Result<SearchPage>::ok(SearchPage{parsed.value(), std::nullopt});
        }
        return Result<SearchPage>::err(res.error());
    }
    return parseLibraryPage(res.value());
}

Result<YouTubeClient::SearchPage> YouTubeClient::getPlaylistsPage(std::optional<std::string_view> continuation) {
    std::string body = buildPlaylistsBrowseBody(innertubeConfig_, continuation);
    auto res = innertubePost("browse", body);
    if (res.isErr()) {
        if (res.error().kind == ErrorKind::Network && useMockSearch_) {
            std::string demo = R"json({"results":[
                {"type":"playlist","id":"p1","title":"Chill Mix","subtitle":"YouTube Music • 50 tracks"},
                {"type":"playlist","id":"p2","title":"Road Trip","subtitle":"You • 34 tracks"},
                {"type":"playlist","id":"p3","title":"Workout","subtitle":"You • 42 tracks"},
                {"type":"playlist","id":"p4","title":"Liked Songs","subtitle":"You • 128 tracks"}
            ]})json";
            auto parsed = parseSearchResponse(demo);
            if (parsed.isOk()) return Result<SearchPage>::ok(SearchPage{parsed.value(), std::nullopt});
        }
        return Result<SearchPage>::err(res.error());
    }
    return parsePlaylistsPage(res.value());
}

Result<YouTubeClient::SearchPage> YouTubeClient::getHistoryPage(std::optional<std::string_view> continuation) {
    std::string body = buildHistoryBrowseBody(innertubeConfig_, continuation);
    auto res = innertubePost("browse", body);
    if (res.isErr()) {
        if (res.error().kind == ErrorKind::Network && useMockSearch_) {
            std::string demo = R"json({"results":[
                {"type":"song","id":"h1","title":"Bohemian Rhapsody","subtitle":"Queen — played 2h ago"},
                {"type":"song","id":"h2","title":"Hotel California","subtitle":"Eagles — played 5h ago"},
                {"type":"song","id":"h3","title":"Viva La Vida","subtitle":"Coldplay — played yesterday"},
                {"type":"album","id":"ha1","title":"Parachutes","subtitle":"Coldplay — viewed yesterday"}
            ]})json";
            auto parsed = parseSearchResponse(demo);
            if (parsed.isOk()) return Result<SearchPage>::ok(SearchPage{parsed.value(), std::nullopt});
        }
        return Result<SearchPage>::err(res.error());
    }
    return parseHistoryPage(res.value());
}

Result<models::SearchResults> YouTubeClient::getLibrary()
{
    auto page = getLibraryPage(std::nullopt);
    if (page.isErr()) return Result<models::SearchResults>::err(page.error());
    return Result<models::SearchResults>::ok(page.value().results);
}

Result<models::SearchResults> YouTubeClient::getPlaylists()
{
    auto page = getPlaylistsPage(std::nullopt);
    if (page.isErr()) return Result<models::SearchResults>::err(page.error());
    return Result<models::SearchResults>::ok(page.value().results);
}

Result<models::SearchResults> YouTubeClient::getHistory()
{
    auto page = getHistoryPage(std::nullopt);
    if (page.isErr()) return Result<models::SearchResults>::err(page.error());
    return Result<models::SearchResults>::ok(page.value().results);
}

void YouTubeClient::attachAuth(HttpRequest& req) const {
    if (authHeaderProvider_) {
        if (auto h = authHeaderProvider_()) req.headers["Authorization"] = *h;
    }
}

Result<models::SearchResults> YouTubeClient::search(std::string_view query, SearchFilter filter)
{
    auto page = searchPage(query, std::nullopt, filter);
    if (page.isErr()) {
        // Fallback for offline/mock when Innertube unreachable
        if (page.error().kind == ErrorKind::Network && useMockSearch_) {
            return fallbackLocalSearch(query, filter);
        }
        return Result<models::SearchResults>::err(page.error());
    }
    auto results = page.value().results;
    // Mock fallback filtering already handled in parse; for flat mock demo, filter here
    if (useMockSearch_) {
        // Only filter if results look like mock flat (no continuation and small catalog)
        // Real InnerTube already filters server-side, so skip when continuation present or results large
        bool needsFilter = !page.value().continuationToken.has_value() && results.size() <= 8;
        if (needsFilter) {
            models::SearchResults filtered;
            for (auto& r : results) {
                bool matchesQuery = query.empty() || containsCi(r.title, query) || containsCi(r.subtitle, query) || containsCi(r.typeLabel(), query);
                bool matchesFilter = filter == SearchFilter::All ||
                    (filter == SearchFilter::Songs && r.type == models::SearchResultType::Song) ||
                    (filter == SearchFilter::Videos && r.type == models::SearchResultType::Song) ||
                    (filter == SearchFilter::Albums && r.type == models::SearchResultType::Album) ||
                    (filter == SearchFilter::Artists && r.type == models::SearchResultType::Artist) ||
                    (filter == SearchFilter::Playlists && r.type == models::SearchResultType::Playlist);
                if (matchesQuery && matchesFilter) filtered.push_back(r);
            }
            return Result<models::SearchResults>::ok(std::move(filtered));
        }
    }
    return Result<models::SearchResults>::ok(std::move(results));
}

Result<YouTubeClient::SearchPage> YouTubeClient::parseSearchPage(std::string_view body) {
    if (body.empty()) return Result<SearchPage>::err(Error::parse("Empty response"));
    // Strip XSSI prefix handled by json::parse
    // Try flat mock first if body looks like flat mock (contains "results" but not renderers)
    bool hasRenderers = body.find("musicResponsiveListItemRenderer") != std::string_view::npos ||
                        body.find("musicTwoRowItemRenderer") != std::string_view::npos;
    if (!hasRenderers) {
        auto flat = parseSearchResponse(body);
        if (flat.isOk()) return Result<SearchPage>::ok(SearchPage{flat.value(), std::nullopt});
        // If flat failed and no renderers, propagate flat error (malformed)
        if (body.find("\"results\"") != std::string_view::npos) return Result<SearchPage>::err(flat.error());
    }
    auto parsed = json::parse(body);
    if (!parsed.ok) return Result<SearchPage>::err(Error::parse(parsed.error));
    if (auto apiErr = extractApiError(body)) return Result<SearchPage>::err(Error::parse(*apiErr));
    // Use nested InnerTube parsing (handles empty results gracefully)
    auto page = parseInnerTubePage(parsed.value);
    return Result<SearchPage>::ok(std::move(page));
}
Result<YouTubeClient::SearchPage> YouTubeClient::parseLibraryPage(std::string_view body) {
    if (body.empty()) return Result<SearchPage>::err(Error::parse("Empty response"));
    bool hasRenderers = body.find("musicResponsiveListItemRenderer") != std::string_view::npos ||
                        body.find("musicTwoRowItemRenderer") != std::string_view::npos ||
                        body.find("gridRenderer") != std::string_view::npos;
    if (!hasRenderers) {
        auto flat = parseSearchResponse(body);
        if (flat.isOk()) return Result<SearchPage>::ok(SearchPage{flat.value(), std::nullopt});
    }
    auto parsed = json::parse(body);
    if (!parsed.ok) return Result<SearchPage>::err(Error::parse(parsed.error));
    if (auto apiErr = extractApiError(body)) return Result<SearchPage>::err(Error::parse(*apiErr));
    auto page = parseInnerTubePage(parsed.value);
    return Result<SearchPage>::ok(std::move(page));
}
Result<YouTubeClient::SearchPage> YouTubeClient::parsePlaylistsPage(std::string_view body) { return parseLibraryPage(body); }
Result<YouTubeClient::SearchPage> YouTubeClient::parseHistoryPage(std::string_view body) { return parseLibraryPage(body); }

} // namespace myytm::youtube
