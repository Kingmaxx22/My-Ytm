#include <iostream>
#include <string>
#include <vector>

#include "youtube/youtube_client.h"
#include "youtube/json.h"
#include "youtube/innertube.h"

using namespace myytm;

static int passed = 0, failed = 0;
#define EXPECT(cond, msg) do { if(cond){++passed; std::cout<<"[PASS] "<<msg<<"\n";} else {++failed; std::cout<<"[FAIL] "<<msg<<" ("#cond")\n";} } while(0)

void test_flat_mock_still_works() {
    std::cout<<"\n-- flat mock still works --\n";
    auto res = youtube::YouTubeClient::parseSearchResponse(R"({"results":[{"type":"song","id":"s1","title":"T","subtitle":"A"}]})");
    EXPECT(res.isOk() && res.value().size()==1, "flat mock parse ok");
    auto page = youtube::YouTubeClient::parseSearchPage(R"({"results":[{"type":"song","id":"s1","title":"T","subtitle":"A"}]})");
    EXPECT(page.isOk() && page.value().results.size()==1, "flat page ok");
}

void test_innerTube_search_nested() {
    std::cout<<"\n-- innerTube search nested --\n";
    std::string json = R"json({
  "contents": {
    "tabbedSearchResultsRenderer": {
      "tabs": [{
        "tabRenderer": {
          "content": {
            "sectionListRenderer": {
              "contents": [
                {
                  "musicShelfRenderer": {
                    "title": {"runs": [{"text": "Songs"}]},
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Blinding Lights"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "The Weeknd • After Hours"}]}}}
                          ],
                          "fixedColumns": [{"musicResponsiveListItemFixedColumnRenderer": {"text": {"runs": [{"text": "3:22"}]}}}],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/abc123/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "xyz123"}}
                        }
                      },
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Save Your Tears"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "The Weeknd"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/t2.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "def456"}}
                        }
                      }
                    ],
                    "continuations": [{"nextContinuationData": {"continuation": "contToken123", "clickTrackingParams": "abc"}}]
                  }
                },
                {
                  "musicShelfRenderer": {
                    "title": {"runs": [{"text": "Artists"}]},
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "The Weeknd"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/artist.jpg"}]}}},
                          "navigationEndpoint": {"browseEndpoint": {"browseId": "UC123"}}
                        }
                      }
                    ]
                  }
                }
              ]
            }
          }
        }
      }]
    }
  }
})json";
    auto page = youtube::YouTubeClient::parseSearchPage(json);
    EXPECT(page.isOk(), "nested search parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==3, "found 3 results (2 songs + 1 artist)");
        bool foundSong = false, foundArtist = false;
        for (auto& r : page.value().results) {
            if (r.title=="Blinding Lights" && r.id=="xyz123") {
                foundSong=true;
                EXPECT(r.type==models::SearchResultType::Song, "song type");
                EXPECT(r.subtitle=="The Weeknd • After Hours", "song subtitle");
                auto* song = std::get_if<models::Song>(&r.payload);
                EXPECT(song && song->durationSeconds.has_value() && *song->durationSeconds==202, "duration 3:22 -> 202");
                EXPECT(song && song->thumbnailUrl.has_value(), "thumbnail present");
            }
            if (r.title=="The Weeknd" && r.id=="UC123") foundArtist=true;
        }
        EXPECT(foundSong, "found Blinding Lights");
        EXPECT(foundArtist, "found artist");
        EXPECT(page.value().continuationToken.has_value() && *page.value().continuationToken=="contToken123", "continuation token");
    }
}

void test_twoRow_and_artist_album_playlist() {
    std::cout<<"\n-- twoRow types --\n";
    std::string json = R"json({
  "contents": {
    "tabbedSearchResultsRenderer": {
      "tabs": [{
        "tabRenderer": {
          "content": {
            "sectionListRenderer": {
              "contents": [{
                "musicShelfRenderer": {
                  "contents": [
                    {
                      "musicTwoRowItemRenderer": {
                        "title": {"runs": [{"text": "Chill Mix"}]},
                        "subtitle": {"runs": [{"text": "Playlist • YouTube Music"}]},
                        "navigationEndpoint": {"browseEndpoint": {"browseId": "VLPL123"}},
                        "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/pl.jpg"}]}}}
                      }
                    },
                    {
                      "musicTwoRowItemRenderer": {
                        "title": {"runs": [{"text": "After Hours"}]},
                        "subtitle": {"runs": [{"text": "Album • The Weeknd"}]},
                        "navigationEndpoint": {"browseEndpoint": {"browseId": "MPREabc"}},
                        "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/al.jpg"}]}}}
                      }
                    }
                  ]
                }
              }]
            }
          }
        }
      }]
    }
  }
})json";
    auto page = youtube::YouTubeClient::parseSearchPage(json);
    EXPECT(page.isOk(), "twoRow parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "2 twoRow results");
        bool foundPl=false, foundAl=false;
        for(auto& r: page.value().results){
            if(r.id=="VLPL123") { foundPl=true; EXPECT(r.type==models::SearchResultType::Playlist, "playlist type"); }
            if(r.id=="MPREabc") { foundAl=true; EXPECT(r.type==models::SearchResultType::Album, "album type"); }
        }
        EXPECT(foundPl && foundAl, "found playlist and album");
    }
}

void test_library_grid() {
    std::cout<<"\n-- library grid --\n";
    std::string json = R"json({
  "contents": {"singleColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [
    {"gridRenderer": {"items": [
      {"musicTwoRowItemRenderer": {"title": {"runs": [{"text": "Liked Album"}]}, "subtitle": {"runs": [{"text": "Artist"}]}, "navigationEndpoint": {"browseEndpoint": {"browseId": "MPRE123"}}, "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/a.jpg"}]}}}}}
    ]}},
    {"musicShelfRenderer": {"title": {"runs": [{"text": "Songs"}]}, "contents": [
      {"musicResponsiveListItemRenderer": {"flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Song A"}]}}}], "navigationEndpoint": {"watchEndpoint": {"videoId": "vid1"}}}}
    ]}}
  ]}}}}]}}
})json";
    auto page = youtube::YouTubeClient::parseLibraryPage(json);
    EXPECT(page.isOk(), "library grid parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "library 2 results (grid + shelf)");
        bool foundAlbum=false, foundSong=false;
        for(auto& r: page.value().results){
            if(r.id=="MPRE123") foundAlbum=true;
            if(r.id=="vid1") foundSong=true;
        }
        EXPECT(foundAlbum && foundSong, "library grid + shelf");
    }
}

void test_library_pagination() {
    std::cout<<"\n-- library pagination --\n";
    // Mock that returns first page with continuation, second page without
    struct PaginatedMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override {
            bool isCont = req.body.find("tok123") != std::string::npos;
            if (isCont) {
                std::string body = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song2"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id2"}}}}]}}]}}}}]}}})json";
                return youtube::HttpResponse{200, body, {}, ""};
            } else {
                std::string body = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song1"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}],"continuations":[{"nextContinuationData":{"continuation":"tok123"}}]}}]}}}}]}}})json";
                return youtube::HttpResponse{200, body, {}, ""};
            }
        }
    };
    auto client = std::make_shared<youtube::YouTubeClient>(std::make_unique<PaginatedMock>());
    auto p1 = client->getLibraryPage(std::nullopt);
    EXPECT(p1.isOk() && p1.value().results.size()==1 && p1.value().continuationToken.has_value(), "library page1 with token");
    auto p2 = client->getLibraryPage(*p1.value().continuationToken);
    EXPECT(p2.isOk() && p2.value().results.size()==1 && !p2.value().continuationToken.has_value(), "library page2 no token");
    // Verify append preserves: simulate UI append
    std::vector<models::SearchResult> combined = p1.value().results;
    combined.insert(combined.end(), p2.value().results.begin(), p2.value().results.end());
    EXPECT(combined.size()==2 && combined[0].id=="id1" && combined[1].id=="id2", "library append preserves");
    // Malformed continuation should be handled gracefully (no token, not error)
    struct MalformedMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest&) override {
            std::string body = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[],"continuations":[{"nextContinuationData":{}}]}}]}}}}]}}})json";
            return youtube::HttpResponse{200, body, {}, ""};
        }
    };
    auto client2 = std::make_shared<youtube::YouTubeClient>(std::make_unique<MalformedMock>());
    auto p3 = client2->getLibraryPage(std::nullopt);
    EXPECT(p3.isOk() && !p3.value().continuationToken.has_value(), "malformed continuation -> no token");
}

void test_missing_optional_fields() {
    std::cout<<"\n-- missing optional fields --\n";
    std::string json = R"json({
  "contents": {"tabbedSearchResultsRenderer": {"tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [{"musicShelfRenderer": {"contents": [
    {"musicResponsiveListItemRenderer": {"flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Only Title"}]}}}], "navigationEndpoint": {"watchEndpoint": {"videoId": "id1"}}}},
    {"musicResponsiveListItemRenderer": {"flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": ""}]}}}], "navigationEndpoint": {"watchEndpoint": {"videoId": "id2"}}}},
    {"unknownRenderer": {"foo":"bar"}}
  ]}}]}}}}]}}
})json";
    auto page = youtube::YouTubeClient::parseSearchPage(json);
    EXPECT(page.isOk(), "missing fields parse ok");
    if (page.isOk()) {
        // First item has only title, no subtitle/duration/thumb -> should still parse
        // Second item has empty title -> should be skipped
        // Third unknown renderer -> ignored
        EXPECT(page.value().results.size()==1, "only 1 valid result");
        if (!page.value().results.empty()) EXPECT(page.value().results[0].title=="Only Title", "title correct");
    }
}

void test_malformed_json() {
    std::cout<<"\n-- malformed json --\n";
    auto r1 = youtube::YouTubeClient::parseSearchPage("{ not json");
    EXPECT(r1.isErr() && r1.error().kind==youtube::ErrorKind::Parse, "malformed json parse error");
    auto r2 = youtube::YouTubeClient::parseSearchPage("");
    EXPECT(r2.isErr(), "empty body parse error");
    auto r3 = youtube::YouTubeClient::parseSearchPage(")]}'\n{ not json");
    EXPECT(r3.isErr(), "XSSI malformed still error");
}

void test_continuation_parsing() {
    std::cout<<"\n-- continuation parsing --\n";
    std::string noCont = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"T"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}]}}]}}}}]}}})json";
    auto p1 = youtube::YouTubeClient::parseSearchPage(noCont);
    EXPECT(p1.isOk() && !p1.value().continuationToken.has_value(), "no continuation");

    std::string withCont = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"T"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}],"continuations":[{"nextContinuationData":{"continuation":"tok123"}}]}}]}}}}]}}})json";
    auto p2 = youtube::YouTubeClient::parseSearchPage(withCont);
    EXPECT(p2.isOk() && p2.value().continuationToken.has_value() && *p2.value().continuationToken=="tok123", "continuation token");

    std::string malformedCont = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[],"continuations":[{"nextContinuationData":{}}]}}]}}}}]}}})json";
    auto p3 = youtube::YouTubeClient::parseSearchPage(malformedCont);
    EXPECT(p3.isOk() && !p3.value().continuationToken.has_value(), "malformed continuation -> no token, not error");
}

void test_pagination_append() {
    std::cout<<"\n-- pagination append --\n";
    std::string page1Json = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song1"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}],"continuations":[{"nextContinuationData":{"continuation":"tok1"}}]}}]}}}}]}}})json";
    std::string page2Json = R"json({"continuationContents":{"musicShelfContinuation":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song2"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id2"}}}}],"continuations":[]}}})json";
    auto p1 = youtube::YouTubeClient::parseSearchPage(page1Json);
    auto p2 = youtube::YouTubeClient::parseSearchPage(page2Json);
    EXPECT(p1.isOk() && p2.isOk(), "both pages parse");
    if (p1.isOk() && p2.isOk()) {
        std::vector<models::SearchResult> combined = p1.value().results;
        combined.insert(combined.end(), p2.value().results.begin(), p2.value().results.end());
        EXPECT(combined.size()==2 && combined[0].id=="id1" && combined[1].id=="id2", "append without corruption");
        EXPECT(p1.value().continuationToken.has_value(), "page1 has token");
        EXPECT(!p2.value().continuationToken.has_value(), "page2 no token (stop)");
    }
}

void test_api_error_response() {
    std::cout<<"\n-- api error response --\n";
    std::string errJson = R"json({"error":{"code":403,"message":"The request is missing a valid API key.","status":"PERMISSION_DENIED"}})json";
    auto r = youtube::YouTubeClient::parseSearchPage(errJson);
    EXPECT(r.isErr() && r.error().kind==youtube::ErrorKind::Parse, "api error -> parse error");
    // Also test via innertubePost error path: simulate via MockHttpClient with 403
    auto http = std::make_unique<youtube::MockHttpClient>();
    http->cannedResponse = youtube::HttpResponse{403, errJson, {}, ""};
    auto client = youtube::YouTubeClient(std::move(http));
    auto res = client.search("test");
    EXPECT(res.isErr() && res.error().kind==youtube::ErrorKind::Auth, "403 -> Auth");

    auto http2 = std::make_unique<youtube::MockHttpClient>();
    http2->cannedResponse = youtube::HttpResponse{200, errJson, {}, ""};
    auto client2 = youtube::YouTubeClient(std::move(http2));
    // innertubePost will check extractApiError even on 200
    auto res2 = client2.search("test");
    EXPECT(res2.isErr(), "200 with error field -> error");
}

void test_empty_search_response() {
    std::cout<<"\n-- empty search --\n";
    std::string empty = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[]}}}}]}}})json";
    auto r = youtube::YouTubeClient::parseSearchPage(empty);
    EXPECT(r.isOk() && r.value().results.empty() && !r.value().continuationToken.has_value(), "empty search -> empty results not error");

    std::string noContents = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[]}}})json";
    auto r2 = youtube::YouTubeClient::parseSearchPage(noContents);
    EXPECT(r2.isOk() && r2.value().results.empty(), "no contents -> empty");

    std::string xssiEmpty = ")]}'\n" + empty;
    auto r3 = youtube::YouTubeClient::parseSearchPage(xssiEmpty);
    EXPECT(r3.isOk(), "XSSI prefix stripped");
}

void test_innertube_url_and_body() {
    std::cout<<"\n-- innertube request --\n";
    youtube::InnertubeConfig cfg;
    cfg.apiKey = "TESTKEY123";
    cfg.clientName = "WEB_REMIX";
    cfg.clientVersion = "1.20240702.01.00";
    std::string url = youtube::buildInnertubeUrl(cfg, "search");
    EXPECT(url.find("https://music.youtube.com/youtubei/v1/search")!=std::string::npos, "url endpoint");
    EXPECT(url.find("key=TESTKEY123")!=std::string::npos, "url api key");
    EXPECT(url.find("prettyPrint=false")!=std::string::npos, "prettyPrint");
    std::string body = youtube::buildSearchBody(cfg, "hello world", std::nullopt);
    EXPECT(body.find("\"query\":\"hello world\"")!=std::string::npos, "body query");
    EXPECT(body.find("\"clientName\":\"WEB_REMIX\"")!=std::string::npos, "client context");
    std::string bodyCont = youtube::buildSearchBody(cfg, "", std::string("tok123"));
    EXPECT(bodyCont.find("\"continuation\":\"tok123\"")!=std::string::npos, "continuation in body");
    EXPECT(bodyCont.find("\"query\"")==std::string::npos, "no query when continuation");
    // Ensure apiKey not in body (only url)
    EXPECT(body.find("TESTKEY123")==std::string::npos, "apiKey not in body");
}

void test_innertube_auth_context_hardening() {
    std::cout<<"\n-- innertube auth/context hardening --\n";
    // Verify WEB_REMIX defaults
    youtube::InnertubeConfig def;
    EXPECT(def.clientName=="WEB_REMIX", "default WEB_REMIX");
    EXPECT(def.clientVersion=="1.20240702.01.00", "default clientVersion");
    EXPECT(def.baseUrl=="https://music.youtube.com", "default baseUrl");
    EXPECT(def.hl=="en" && def.gl=="US", "default hl/gl");
    // visitorData in context when available
    youtube::InnertubeConfig cfg;
    cfg.visitorData = "CgtTestVisitor123%3D";
    std::string bodyWithVd = youtube::buildSearchBody(cfg, "test", std::nullopt);
    EXPECT(bodyWithVd.find("\"visitorData\":\"CgtTestVisitor123%3D\"")!=std::string::npos, "visitorData in context");
    youtube::InnertubeConfig cfgNoVd;
    std::string bodyNoVd = youtube::buildSearchBody(cfgNoVd, "test", std::nullopt);
    EXPECT(bodyNoVd.find("visitorData")==std::string::npos, "no visitorData when empty");

    // X-Goog-Api-Key and authenticated headers via captured request
    struct CapturingMock : public youtube::IHttpClient {
        youtube::HttpRequest lastReq;
        youtube::HttpResponse toReturn{200, R"({"contents":{"tabbedSearchResultsRenderer":{"tabs":[]}}})", {}, ""};
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override { lastReq = req; return toReturn; }
    };
    auto cap = std::make_unique<CapturingMock>();
    auto* rawCap = cap.get();
    youtube::InnertubeConfig cfg2; cfg2.apiKey="MYKEY123"; cfg2.visitorData="VISITOR123";
    auto client = youtube::YouTubeClient(std::move(cap));
    client.setInnertubeConfig(cfg2);
    client.setAuthHeaderProvider([]()->std::optional<std::string>{ return std::string("Bearer Tok123"); });
    auto res = client.search("hello");
    EXPECT(rawCap->lastReq.url.find("key=MYKEY123")!=std::string::npos, "X-Goog-Api-Key in URL (via ?key=)");
    EXPECT(rawCap->lastReq.headers.count("X-Goog-Api-Key") && rawCap->lastReq.headers.at("X-Goog-Api-Key")=="MYKEY123", "X-Goog-Api-Key header");
    EXPECT(rawCap->lastReq.headers.count("Authorization") && rawCap->lastReq.headers.at("Authorization")=="Bearer Tok123", "Authorization header");
    EXPECT(rawCap->lastReq.headers.count("X-Goog-Visitor-Id") || rawCap->lastReq.body.find("VISITOR123")!=std::string::npos, "visitorData in header or body");
    EXPECT(rawCap->lastReq.body.find("VISITOR123")!=std::string::npos, "visitorData in body context");
    EXPECT(rawCap->lastReq.body.find("Tok123")==std::string::npos, "token not in body (only header)");
    EXPECT(rawCap->lastReq.headers.at("X-Goog-Api-Key").find("Tok123")==std::string::npos, "token not leaked in api key header");

    // Unauthenticated: no Authorization header
    struct CapturingMock2 : public youtube::IHttpClient {
        youtube::HttpRequest lastReq;
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override { lastReq=req; return youtube::HttpResponse{200, R"({"contents":{}})", {}, ""}; }
    };
    auto cap2 = std::make_unique<CapturingMock2>();
    auto* raw2 = cap2.get();
    youtube::YouTubeClient client2(std::move(cap2));
    client2.setInnertubeConfig(cfg2);
    // No auth provider
    client2.search("hello");
    EXPECT(raw2->lastReq.headers.find("Authorization")==raw2->lastReq.headers.end(), "no auth header when signed out");

    // visitorData extraction and persistence in-memory
    struct VisitorMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest&) override {
            std::string body = R"({"responseContext":{"visitorData":"CgtNewVisitor%3D"},"contents":{}})";
            return youtube::HttpResponse{200, body, {}, ""};
        }
    };
    auto vClient = youtube::YouTubeClient(std::make_unique<VisitorMock>());
    youtube::InnertubeConfig vCfg; vCfg.visitorData="";
    vClient.setInnertubeConfig(vCfg);
    // Before request, no visitorData
    EXPECT(vClient.innertubeConfig().visitorData.empty(), "initial visitorData empty");
    vClient.search("test");
    EXPECT(vClient.innertubeConfig().visitorData=="CgtNewVisitor%3D", "visitorData captured in-memory");
    // Next request should include it
    struct CapturingMock3 : public youtube::IHttpClient {
        youtube::HttpRequest lastReq;
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override { lastReq=req; return youtube::HttpResponse{200, R"({"contents":{}})", {}, ""}; }
    };
    // Use same client (already has visitorData) to make next request, capture body
    auto cap3 = std::make_unique<CapturingMock3>();
    auto* raw3 = cap3.get();
    // Need to transfer visitorData to new client for test isolation: simulate persistence
    youtube::InnertubeConfig cfgWithVd; cfgWithVd.visitorData="CgtPersisted%3D";
    auto client3 = youtube::YouTubeClient(std::move(cap3));
    client3.setInnertubeConfig(cfgWithVd);
    client3.search("hello2");
    EXPECT(raw3->lastReq.body.find("CgtPersisted%3D")!=std::string::npos, "persisted visitorData in next request");

    // 401/403/429 handling without retry
    auto http401 = std::make_unique<youtube::MockHttpClient>();
    http401->cannedResponse = youtube::HttpResponse{401, R"({"error":{"code":401,"message":"Unauthorized"}})", {}, ""};
    auto c401 = youtube::YouTubeClient(std::move(http401));
    auto r401 = c401.search("q");
    EXPECT(r401.isErr() && r401.error().kind==youtube::ErrorKind::Auth, "401 -> Auth (no retry)");

    auto http403 = std::make_unique<youtube::MockHttpClient>();
    http403->cannedResponse = youtube::HttpResponse{403, R"({"error":{"code":403,"message":"Forbidden"}})", {}, ""};
    auto c403 = youtube::YouTubeClient(std::move(http403));
    auto r403 = c403.search("q");
    EXPECT(r403.isErr() && r403.error().kind==youtube::ErrorKind::Auth, "403 -> Auth");

    auto http429 = std::make_unique<youtube::MockHttpClient>();
    http429->cannedResponse = youtube::HttpResponse{429, "", {}, ""};
    auto c429 = youtube::YouTubeClient(std::move(http429));
    auto r429 = c429.search("q");
    EXPECT(r429.isErr() && r429.error().kind==youtube::ErrorKind::RateLimited, "429 -> RateLimited");
    // Ensure no secret in error message
    EXPECT(r401.error().message.find("MYKEY123")==std::string::npos && r401.error().message.find("Tok123")==std::string::npos, "no secret in 401 error");
}

void test_search_filter() {
    std::cout<<"\n-- search filter Songs --\n";
    // Params generation
    auto paramsAll = youtube::searchFilterParams(youtube::SearchFilter::All);
    EXPECT(!paramsAll.has_value(), "All has no params");
    auto paramsSongs = youtube::searchFilterParams(youtube::SearchFilter::Songs);
    EXPECT(paramsSongs.has_value() && *paramsSongs=="EgWKAQIIAWoKEAoQAxAEEAkQBQ==", "Songs params correct");
    EXPECT(youtube::searchFilterLabel(youtube::SearchFilter::Songs)=="Songs", "label Songs");
    EXPECT(youtube::searchFilterLabel(youtube::SearchFilter::All)=="All", "label All");

    // Request body with filter
    youtube::InnertubeConfig cfg;
    cfg.apiKey="TESTKEY"; cfg.clientName="WEB_REMIX"; cfg.clientVersion="1.20240702.01.00";
    std::string bodyAll = youtube::buildSearchBody(cfg, "test", std::nullopt, youtube::SearchFilter::All);
    EXPECT(bodyAll.find("\"params\"")==std::string::npos, "All body no params");
    std::string bodySongs = youtube::buildSearchBody(cfg, "test", std::nullopt, youtube::SearchFilter::Songs);
    EXPECT(bodySongs.find("\"params\":\"EgWKAQIIAWoKEAoQAxAEEAkQBQ==\"")!=std::string::npos, "Songs body has params");
    EXPECT(bodySongs.find("\"query\":\"test\"")!=std::string::npos, "Songs body still has query");
    // Continuation should not include query/params, only continuation
    std::string bodyContFiltered = youtube::buildSearchBody(cfg, "test", std::string("tok"), youtube::SearchFilter::Songs);
    EXPECT(bodyContFiltered.find("\"continuation\":\"tok\"")!=std::string::npos, "filtered continuation");
    EXPECT(bodyContFiltered.find("\"params\"")==std::string::npos, "continuation no params (token encodes filter)");
    EXPECT(bodyContFiltered.find("\"query\"")==std::string::npos, "continuation no query");

    // YouTubeClient filtered search with mock — should return only Songs
    auto client = std::make_shared<youtube::YouTubeClient>(std::make_unique<youtube::MockHttpClient>());
    auto resAll = client->search("Blinding", youtube::SearchFilter::All);
    EXPECT(resAll.isOk(), "unfiltered search ok");
    auto resSongs = client->search("Blinding", youtube::SearchFilter::Songs);
    EXPECT(resSongs.isOk(), "filtered Songs search ok");
    if (resSongs.isOk()) {
        bool allSongs = true;
        for(auto& r: resSongs.value()) if(r.type!=models::SearchResultType::Song) allSongs=false;
        EXPECT(allSongs, "Songs filter returns only Songs");
        EXPECT(resSongs.value().size() < resAll.value().size() || resSongs.value().size()==resAll.value().size(), "filtered <= unfiltered");
    }
    // Parsing filtered nested response: should still parse correctly (songs only)
    std::string filteredJson = R"json({
  "contents": {"tabbedSearchResultsRenderer": {"tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [{"musicShelfRenderer": {"title": {"runs": [{"text": "Songs"}]}, "contents": [
    {"musicResponsiveListItemRenderer": {"flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Filtered Song"}]}}}], "navigationEndpoint": {"watchEndpoint": {"videoId": "vidFiltered"}}}}
  ]}}]}}}}]}}
})json";
    auto page = youtube::YouTubeClient::parseSearchPage(filteredJson);
    EXPECT(page.isOk() && page.value().results.size()==1 && page.value().results[0].title=="Filtered Song", "filtered parsing ok");
}

int main(){
    test_flat_mock_still_works();
    test_innerTube_search_nested();
    test_twoRow_and_artist_album_playlist();
    test_library_grid();
    test_library_pagination();
    test_missing_optional_fields();
    test_malformed_json();
    test_continuation_parsing();
    test_pagination_append();
    test_api_error_response();
    test_empty_search_response();
    test_innertube_url_and_body();
    test_search_filter();
    test_innertube_auth_context_hardening();
    std::cout<<"\n=== YouTube InnerTube Tests: "<<passed<<" passed, "<<failed<<" failed ===\n";
    return failed==0?0:1;
}
