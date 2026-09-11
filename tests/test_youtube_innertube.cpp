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

void test_all_filters() {
    std::cout<<"\n-- all search filters --\n";
    struct Case { youtube::SearchFilter f; const char* label; const char* params; models::SearchResultType expectedType; };
    std::vector<Case> cases = {
        {youtube::SearchFilter::All, "All", nullptr, models::SearchResultType::Song},
        {youtube::SearchFilter::Songs, "Songs", "EgWKAQIIAWoKEAoQAxAEEAkQBQ==", models::SearchResultType::Song},
        {youtube::SearchFilter::Videos, "Videos", "EgWKAQIQAWoKEAoQAxAEEAkQBQ==", models::SearchResultType::Song},
        {youtube::SearchFilter::Albums, "Albums", "EgWKAQIYAWoKEAoQAxAEEAkQBQ==", models::SearchResultType::Album},
        {youtube::SearchFilter::Artists, "Artists", "EgWKAQEgAWoKEAoQAxAEEAkQBQ==", models::SearchResultType::Artist},
        {youtube::SearchFilter::Playlists, "Playlists", "EgWKAQIoAWoKEAoQAxAEEAkQBQ==", models::SearchResultType::Playlist},
    };
    for (auto& c : cases) {
        EXPECT(youtube::searchFilterLabel(c.f)==c.label, std::string("label ")+c.label);
        auto p = youtube::searchFilterParams(c.f);
        if (c.params) { EXPECT(p.has_value() && *p==c.params, std::string("params ")+c.label); }
        else { EXPECT(!p.has_value(), "All no params"); }
        youtube::InnertubeConfig cfg; cfg.apiKey="K"; cfg.clientName="WEB_REMIX"; cfg.clientVersion="1.20240702.01.00";
        std::string body = youtube::buildSearchBody(cfg, "q", std::nullopt, c.f);
        if (c.params) EXPECT(body.find(c.params)!=std::string::npos, std::string("body has params ")+c.label);
        else EXPECT(body.find("\"params\"")==std::string::npos, "All no params in body");
        // Fallback filtering: each filter should return only matching type from mock catalog
        auto client = std::make_shared<youtube::YouTubeClient>(std::make_unique<youtube::MockHttpClient>());
        auto res = client->search("", c.f);
        EXPECT(res.isOk(), std::string("search ")+c.label+" ok");
        if (res.isOk() && c.f != youtube::SearchFilter::All) {
            for(auto& r: res.value()) {
                bool ok = (c.f==youtube::SearchFilter::Songs && r.type==models::SearchResultType::Song) ||
                          (c.f==youtube::SearchFilter::Videos && r.type==models::SearchResultType::Song) ||
                          (c.f==youtube::SearchFilter::Albums && r.type==models::SearchResultType::Album) ||
                          (c.f==youtube::SearchFilter::Artists && r.type==models::SearchResultType::Artist) ||
                          (c.f==youtube::SearchFilter::Playlists && r.type==models::SearchResultType::Playlist);
                if (!ok) { EXPECT(false, std::string("type mismatch for ")+c.label); break; }
            }
        }
    }
    // Pagination preserves filter: first page with Songs, next page should still be Songs
    struct PaginatedMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override {
            bool isCont = req.body.find("contTok")!=std::string::npos;
            if (!isCont) {
                bool isSongs = req.body.find("EgWKAQIIAWoKEAoQAxAEEAkQBQ==")!=std::string::npos;
                EXPECT(isSongs, "pagination preserves Songs params in first page");
            }
            if (isCont) {
                std::string b = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song2"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id2"}}}}]}}]}}}}]}}})json";
                return youtube::HttpResponse{200,b,{}, ""};
            } else {
                std::string b = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Song1"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}],"continuations":[{"nextContinuationData":{"continuation":"contTok"}}]}}]}}}}]}}})json";
                return youtube::HttpResponse{200,b,{}, ""};
            }
        }
    };
    auto pagClient = std::make_shared<youtube::YouTubeClient>(std::make_unique<PaginatedMock>());
    auto p1 = pagClient->searchPage("test", std::nullopt, youtube::SearchFilter::Songs);
    EXPECT(p1.isOk() && p1.value().continuationToken.has_value(), "filtered page1 has token");
    auto p2 = pagClient->searchPage("test", *p1.value().continuationToken, youtube::SearchFilter::Songs);
    EXPECT(p2.isOk() && p2.value().results.size()==1 && p2.value().results[0].id=="id2", "filtered page2 ok");
}

void test_library_browse_nested() {
    std::cout<<"\n-- library browse nested --\n";
    // Real Library browse: singleColumnBrowseResultsRenderer → tabs → sectionListRenderer
    // Mix of gridRenderer (albums/artists), musicShelfRenderer (songs), playlistRenderer
    std::string json = R"json({
  "contents": {
    "singleColumnBrowseResultsRenderer": {
      "tabs": [{
        "tabRenderer": {
          "content": {
            "sectionListRenderer": {
              "contents": [
                {
                  "musicShelfRenderer": {
                    "title": {"runs": [{"text": "Liked Songs"}]},
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Bohemian Rhapsody"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Queen"}]}}}
                          ],
                          "fixedColumns": [{"musicResponsiveListItemFixedColumnRenderer": {"text": {"runs": [{"text": "5:55"}]}}}],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/fJ9rUzIMcZQ/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "fJ9rUzIMcZQ"}}
                        }
                      },
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Hotel California"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Eagles"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/Bk7RVwIguZc/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "Bk7RVwIguZc"}}
                        }
                      }
                    ],
                    "continuations": [{"nextContinuationData": {"continuation": "libCont123"}}]
                  }
                },
                {
                  "gridRenderer": {
                    "items": [
                      {
                        "musicTwoRowItemRenderer": {
                          "title": {"runs": [{"text": "After Hours"}]},
                          "subtitle": {"runs": [{"text": "Album • The Weeknd"}]},
                          "navigationEndpoint": {"browseEndpoint": {"browseId": "MPREbQk5Wg"}},
                          "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/after_hours.jpg"}]}}}
                        }
                      },
                      {
                        "musicTwoRowItemRenderer": {
                          "title": {"runs": [{"text": "The Weeknd"}]},
                          "subtitle": {"runs": [{"text": "Artist"}]},
                          "navigationEndpoint": {"browseEndpoint": {"browseId": "UCbA6JPEfmJ45MeAjsPdZkqg"}},
                          "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/weeknd.jpg"}]}}}
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
    auto page = youtube::YouTubeClient::parseLibraryPage(json);
    EXPECT(page.isOk(), "library browse parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==4, "library 4 results (2 songs + 1 album + 1 artist)");
        int songs=0, albums=0, artists=0;
        for (auto& r : page.value().results) {
            if (r.type==models::SearchResultType::Song) {
                ++songs;
                if (r.id=="fJ9rUzIMcZQ") {
                    EXPECT(r.title=="Bohemian Rhapsody", "song title");
                    EXPECT(r.subtitle=="Queen", "song subtitle");
                    auto* s = std::get_if<models::Song>(&r.payload);
                    EXPECT(s && s->durationSeconds.has_value() && *s->durationSeconds==355, "duration 5:55 -> 355");
                    EXPECT(s && s->thumbnailUrl.has_value(), "thumbnail");
                }
                if (r.id=="Bk7RVwIguZc") EXPECT(r.title=="Hotel California", "second song title");
            }
            if (r.type==models::SearchResultType::Album) { ++albums; EXPECT(r.id=="MPREbQk5Wg", "album id"); }
            if (r.type==models::SearchResultType::Artist) { ++artists; EXPECT(r.id=="UCbA6JPEfmJ45MeAjsPdZkqg", "artist id"); }
        }
        EXPECT(songs==2, "2 songs");
        EXPECT(albums==1, "1 album");
        EXPECT(artists==1, "1 artist");
        EXPECT(page.value().continuationToken.has_value() && *page.value().continuationToken=="libCont123", "library continuation");
    }
}

void test_playlists_browse_nested() {
    std::cout<<"\n-- playlists browse nested --\n";
    // Real Playlists browse: mix of playlistRenderer and musicTwoRowItemRenderer in grid
    std::string json = R"json({
  "contents": {
    "singleColumnBrowseResultsRenderer": {
      "tabs": [{
        "tabRenderer": {
          "content": {
            "sectionListRenderer": {
              "contents": [
                {
                  "gridRenderer": {
                    "items": [
                      {
                        "playlistRenderer": {
                          "playlistId": "PLrAXtmErZgOeiKm4sgNOknGvNjby9efdf",
                          "title": {"runs": [{"text": "My Chill Playlist"}]},
                          "shortBylineText": {"runs": [{"text": "Me"}]},
                          "videoCountShort": {"runs": [{"text": "50 videos"}]},
                          "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/chill.jpg"}]}}}
                        }
                      },
                      {
                        "playlistRenderer": {
                          "playlistId": "PLrAXtmErZgOeiKm4sgNOknGvNjby9efdf2",
                          "title": {"simpleText": "Road Trip Mix"},
                          "longBylineText": {"runs": [{"text": "YouTube Music"}]},
                          "videoCount": 34,
                          "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/roadtrip.jpg"}]}}}
                        }
                      },
                      {
                        "musicTwoRowItemRenderer": {
                          "title": {"runs": [{"text": "Liked Songs"}]},
                          "subtitle": {"runs": [{"text": "Playlist • You"}]},
                          "navigationEndpoint": {"browseEndpoint": {"browseId": "VLPLLOlV9EbD9f2b5V7kC6VQJ"}},
                          "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/liked.jpg"}]}}}
                        }
                      }
                    ]
                  }
                },
                {
                  "musicShelfRenderer": {
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Workout Mix"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "You • 42 tracks"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/workout.jpg"}]}}},
                          "navigationEndpoint": {"browseEndpoint": {"browseId": "VLPLworkout123"}}
                        }
                      }
                    ],
                    "continuations": [{"nextContinuationData": {"continuation": "plCont456"}}]
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
    auto page = youtube::YouTubeClient::parsePlaylistsPage(json);
    EXPECT(page.isOk(), "playlists browse parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==4, "playlists 4 results (2 playlistRenderers + 1 twoRow + 1 responsive)");
        int plCount=0;
        for (auto& r : page.value().results) {
            EXPECT(r.type==models::SearchResultType::Playlist, "all playlists type");
            if (r.id=="PLrAXtmErZgOeiKm4sgNOknGvNjby9efdf") {
                EXPECT(r.title=="My Chill Playlist", "playlistRenderer title");
                EXPECT(r.subtitle=="Me", "playlistRenderer author via shortBylineText");
                auto* p = std::get_if<models::Playlist>(&r.payload);
                EXPECT(p && p->trackCount==50, "playlistRenderer trackCount from videoCountShort");
                ++plCount;
            }
            if (r.id=="PLrAXtmErZgOeiKm4sgNOknGvNjby9efdf2") {
                EXPECT(r.title=="Road Trip Mix", "simpleText title");
                EXPECT(r.subtitle=="YouTube Music", "longBylineText author");
                auto* p = std::get_if<models::Playlist>(&r.payload);
                EXPECT(p && p->trackCount==34, "playlistRenderer trackCount from videoCount");
                ++plCount;
            }
            if (r.id=="VLPLLOlV9EbD9f2b5V7kC6VQJ") {
                EXPECT(r.title=="Liked Songs", "twoRow playlist title");
                EXPECT(r.subtitle=="Playlist • You", "twoRow playlist subtitle");
                ++plCount;
            }
            if (r.id=="VLPLworkout123") {
                EXPECT(r.title=="Workout Mix", "responsive playlist title");
                ++plCount;
            }
        }
        EXPECT(plCount==4, "found all 4 playlists");
        EXPECT(page.value().continuationToken.has_value() && *page.value().continuationToken=="plCont456", "playlists continuation");
    }
}

void test_history_browse_nested() {
    std::cout<<"\n-- history browse nested --\n";
    // Real History browse: musicResponsiveListItemRenderer with play songs, musicTwoRowItemRenderer for albums
    std::string json = R"json({
  "contents": {
    "singleColumnBrowseResultsRenderer": {
      "tabs": [{
        "tabRenderer": {
          "content": {
            "sectionListRenderer": {
              "contents": [
                {
                  "musicShelfRenderer": {
                    "title": {"runs": [{"text": "Today"}]},
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Stairway to Heaven"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Led Zeppelin"}]}}}
                          ],
                          "fixedColumns": [{"musicResponsiveListItemFixedColumnRenderer": {"text": {"runs": [{"text": "8:02"}]}}}],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/QkFVsttGmY/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "QkFVsttGmY"}}
                        }
                      },
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Comfortably Numb"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Pink Floyd"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/XXlQ3TsccYs/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "XXlQ3TsccYs"}}
                        }
                      }
                    ]
                  }
                },
                {
                  "musicShelfRenderer": {
                    "title": {"runs": [{"text": "Yesterday"}]},
                    "contents": [
                      {
                        "musicResponsiveListItemRenderer": {
                          "flexColumns": [
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Imagine"}]}}},
                            {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "John Lennon"}]}}}
                          ],
                          "thumbnail": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/DyDfgMOUjCI/hqdefault.jpg"}]}}},
                          "navigationEndpoint": {"watchEndpoint": {"videoId": "DyDfgMOUjCI"}}
                        }
                      }
                    ],
                    "continuations": [{"nextContinuationData": {"continuation": "histCont789"}}]
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
    auto page = youtube::YouTubeClient::parseHistoryPage(json);
    EXPECT(page.isOk(), "history browse parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==3, "history 3 results");
        bool foundStairway=false, foundNumb=false, foundImagine=false;
        for (auto& r : page.value().results) {
            EXPECT(r.type==models::SearchResultType::Song, "history items are songs");
            if (r.id=="QkFVsttGmY") {
                foundStairway=true;
                EXPECT(r.title=="Stairway to Heaven", "history song title");
                EXPECT(r.subtitle=="Led Zeppelin", "history song artist");
                auto* s = std::get_if<models::Song>(&r.payload);
                EXPECT(s && s->durationSeconds.has_value() && *s->durationSeconds==482, "duration 8:02 -> 482");
                EXPECT(s && s->thumbnailUrl.has_value(), "thumbnail present");
            }
            if (r.id=="XXlQ3TsccYs") foundNumb=true;
            if (r.id=="DyDfgMOUjCI") {
                foundImagine=true;
                EXPECT(r.title=="Imagine", "yesterday song");
            }
        }
        EXPECT(foundStairway && foundNumb && foundImagine, "found all 3 history songs");
        EXPECT(page.value().continuationToken.has_value() && *page.value().continuationToken=="histCont789", "history continuation");
    }
}

void test_browse_malformed() {
    std::cout<<"\n-- browse malformed --\n";
    // Library: empty renderer (no title)
    std::string emptyTitle = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":""}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"id1"}}}}]}}]}}}}]}}})json";
    auto r1 = youtube::YouTubeClient::parseLibraryPage(emptyTitle);
    EXPECT(r1.isOk(), "library empty title -> ok (skip entry)");
    EXPECT(r1.isOk() && r1.value().results.empty(), "library empty title -> 0 results");

    // Playlists: missing playlistId in playlistRenderer
    std::string noPlaylistId = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"gridRenderer":{"items":[{"playlistRenderer":{"title":{"runs":[{"text":"No ID"}]},"shortBylineText":{"runs":[{"text":"X"}]}}}]}}]}}}}]}}})json";
    auto r2 = youtube::YouTubeClient::parsePlaylistsPage(noPlaylistId);
    EXPECT(r2.isOk(), "playlists missing playlistId -> ok");
    EXPECT(r2.isOk() && r2.value().results.empty(), "playlists missing playlistId -> 0 results");

    // History: completely empty contents
    std::string emptyHist = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[]}}}}]}}})json";
    auto r3 = youtube::YouTubeClient::parseHistoryPage(emptyHist);
    EXPECT(r3.isOk(), "history empty contents -> ok");
    EXPECT(r3.isOk() && r3.value().results.empty(), "history empty -> 0 results");

    // Library: unknown renderer type (should be ignored)
    std::string unknownType = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"someOtherRenderer":{"data":"ignore"}}]}}}}]}}})json";
    auto r4 = youtube::YouTubeClient::parseLibraryPage(unknownType);
    EXPECT(r4.isOk() && r4.value().results.empty(), "library unknown renderer -> 0 results");

    // All 3: malformed JSON
    EXPECT(youtube::YouTubeClient::parseLibraryPage("{bad").isErr(), "library malformed JSON");
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage("{bad").isErr(), "playlists malformed JSON");
    EXPECT(youtube::YouTubeClient::parseHistoryPage("{bad").isErr(), "history malformed JSON");

    // All 3: empty body
    EXPECT(youtube::YouTubeClient::parseLibraryPage("").isErr(), "library empty body");
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage("").isErr(), "playlists empty body");
    EXPECT(youtube::YouTubeClient::parseHistoryPage("").isErr(), "history empty body");

    // All 3: API error in body
    std::string apiErr = R"json({"error":{"code":403,"message":"Access denied"}})json";
    EXPECT(youtube::YouTubeClient::parseLibraryPage(apiErr).isErr(), "library API error");
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage(apiErr).isErr(), "playlists API error");
    EXPECT(youtube::YouTubeClient::parseHistoryPage(apiErr).isErr(), "history API error");

    // Flat mock backward compat for all 3
    std::string flatMock = R"json({"results":[{"type":"song","id":"s1","title":"T","subtitle":"A"}]})json";
    auto fLib = youtube::YouTubeClient::parseLibraryPage(flatMock);
    EXPECT(fLib.isOk() && fLib.value().results.size()==1, "library flat mock fallback");
    auto fPl = youtube::YouTubeClient::parsePlaylistsPage(flatMock);
    EXPECT(fPl.isOk() && fPl.value().results.size()==1, "playlists flat mock fallback");
    auto fHist = youtube::YouTubeClient::parseHistoryPage(flatMock);
    EXPECT(fHist.isOk() && fHist.value().results.size()==1, "history flat mock fallback");
}

void test_browse_mixed_renderers_and_continuation() {
    std::cout<<"\n-- browse mixed renderers + continuation --\n";
    // Playlists: playlistRenderer + musicResponsiveListItemRenderer + continuation
    std::string json = R"json({
  "contents": {"singleColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [
    {"gridRenderer": {"items": [
      {"playlistRenderer": {"playlistId": "PLaaa", "title": {"runs": [{"text": "Alpha"}]}, "shortBylineText": {"runs": [{"text": "U1"}]}, "videoCountShort": {"runs": [{"text": "10 videos"}]}}},
      {"playlistRenderer": {"playlistId": "PLbbb", "title": {"simpleText": "Beta"}, "videoCount": 25}}
    ]}},
    {"musicShelfRenderer": {"contents": [
      {"musicResponsiveListItemRenderer": {"flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Gamma"}]}}}], "navigationEndpoint": {"browseEndpoint": {"browseId": "VLPLgamma"}}}}
    ], "continuations": [{"nextContinuationData": {"continuation": "mixCont1"}}]}}
  ]}}}}]}}
})json";
    auto page = youtube::YouTubeClient::parsePlaylistsPage(json);
    EXPECT(page.isOk(), "mixed parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==3, "3 playlists total");
        bool a=false, b=false, c=false;
        for (auto& r : page.value().results) {
            EXPECT(r.type==models::SearchResultType::Playlist, "all playlist type");
            if (r.id=="PLaaa") { a=true; auto* p=std::get_if<models::Playlist>(&r.payload); EXPECT(p && p->trackCount==10, "Alpha trackCount"); }
            if (r.id=="PLbbb") { b=true; auto* p=std::get_if<models::Playlist>(&r.payload); EXPECT(p && p->trackCount==25, "Beta trackCount"); }
            if (r.id=="VLPLgamma") { c=true; EXPECT(r.title=="Gamma", "Gamma title"); }
        }
        EXPECT(a && b && c, "found all 3");
        EXPECT(page.value().continuationToken.has_value() && *page.value().continuationToken=="mixCont1", "mixed continuation");
    }
}

void test_history_twoRow_and_empty_shelf() {
    std::cout<<"\n-- history twoRow + empty shelf --\n";
    // History can also have musicTwoRowItemRenderer (for albums viewed) + empty shelves
    std::string json = R"json({
  "contents": {"singleColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [
    {"musicShelfRenderer": {"title": {"runs": [{"text": "Recent"}]}, "contents": [
      {"musicResponsiveListItemRenderer": {"flexColumns": [
        {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "Yesterday"}]}}},
        {"musicResponsiveListItemFlexColumnRenderer": {"text": {"runs": [{"text": "The Beatles"}]}}}
      ], "navigationEndpoint": {"watchEndpoint": {"videoId": "vid1"}}}}
    ]}},
    {"musicShelfRenderer": {"title": {"runs": [{"text": "Older"}]}, "contents": []}},
    {"musicShelfRenderer": {"contents": [
      {"musicTwoRowItemRenderer": {"title": {"runs": [{"text": "Dark Side of the Moon"}]}, "subtitle": {"runs": [{"text": "Album • Pink Floyd"}]}, "navigationEndpoint": {"browseEndpoint": {"browseId": "MPREdark"}}, "thumbnailRenderer": {"musicThumbnailRenderer": {"thumbnail": {"thumbnails": [{"url": "https://example.com/dark.jpg"}]}}}}}
    ]}}
  ]}}}}]}}
})json";
    auto page = youtube::YouTubeClient::parseHistoryPage(json);
    EXPECT(page.isOk(), "history with twoRow + empty shelf ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "2 results (empty shelf skipped)");
        bool song=false, album=false;
        for (auto& r : page.value().results) {
            if (r.id=="vid1") { song=true; EXPECT(r.type==models::SearchResultType::Song, "Yesterday is Song"); EXPECT(r.title=="Yesterday", "Yesterday title"); EXPECT(r.subtitle=="The Beatles", "Yesterday subtitle"); }
            if (r.id=="MPREdark") { album=true; EXPECT(r.type==models::SearchResultType::Album, "Dark Side is Album"); EXPECT(r.title=="Dark Side of the Moon", "Dark Side title"); }
        }
        EXPECT(song && album, "found song and album in history");
    }
}

void test_playlist_item_renderer_normal() {
    std::cout<<"\n-- playlistItemRenderer normal --\n";
    std::string json = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[
      {"playlistItemRenderer":{"videoId":"vidA1","title":{"runs":[{"text":"Song Alpha"}]},"shortBylineText":{"runs":[{"text":"Artist One"}]},"lengthText":{"runs":[{"text":"3:45"}]},"thumbnailRenderer":{"musicThumbnailRenderer":{"thumbnail":{"thumbnails":[{"url":"https://example.com/a.jpg"}]}}},"navigationEndpoint":{"watchEndpoint":{"videoId":"vidA1"}}}},
      {"playlistItemRenderer":{"videoId":"vidB2","title":{"runs":[{"text":"Song Beta"}]},"shortBylineText":{"runs":[{"text":"Artist Two"}]},"lengthText":{"simpleText":"4:02"},"thumbnailRenderer":{"musicThumbnailRenderer":{"thumbnail":{"thumbnails":[{"url":"https://example.com/b.jpg"}]}}},"navigationEndpoint":{"watchEndpoint":{"videoId":"vidB2"}}}}
    ]}}]}}}}]}}})json";
    auto page = youtube::YouTubeClient::parsePlaylistsPage(json);
    EXPECT(page.isOk(), "playlist items parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "2 playlist items");
        bool a=false,b=false;
        for (auto& r : page.value().results) {
            EXPECT(r.type==models::SearchResultType::Song, "item type Song");
            if (r.id=="vidA1") { a=true; EXPECT(r.title=="Song Alpha", "alpha title"); EXPECT(r.subtitle=="Artist One", "alpha author");
                auto* s=std::get_if<models::Song>(&r.payload); EXPECT(s && s->durationSeconds.has_value() && *s->durationSeconds==225, "3:45 -> 225"); EXPECT(s && s->thumbnailUrl.has_value() && *s->thumbnailUrl=="https://example.com/a.jpg", "alpha thumb"); }
            if (r.id=="vidB2") { b=true; EXPECT(r.title=="Song Beta", "beta title"); EXPECT(r.subtitle=="Artist Two", "beta author");
                auto* s=std::get_if<models::Song>(&r.payload); EXPECT(s && s->durationSeconds.has_value() && *s->durationSeconds==242, "4:02 -> 242"); }
        }
        EXPECT(a && b, "found both items");
    }
}

void test_playlist_item_missing_fields() {
    std::cout<<"\n-- playlistItemRenderer missing fields --\n";
    std::string json = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[
      {"playlistItemRenderer":{"title":{"runs":[{"text":"No VideoId"}]},"shortBylineText":{"runs":[{"text":"X"}]}}},
      {"playlistItemRenderer":{"videoId":"vidNoTitle","shortBylineText":{"runs":[{"text":"X"}]}}},
      {"playlistItemRenderer":{"videoId":"vidNavOnly","title":{"runs":[{"text":"Nav Title"}]},"navigationEndpoint":{"watchEndpoint":{"videoId":"vidNavOnly"}}}},
      {"playlistItemRenderer":{"videoId":"vidNoAuthor","title":{"runs":[{"text":"No Author No Dur"}]}}}
    ]}}]}}}}]}}})json";
    auto page = youtube::YouTubeClient::parsePlaylistsPage(json);
    EXPECT(page.isOk(), "missing fields parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "2 valid (nav-id fallback + no-author)");
        bool nav=false,noauth=false;
        for (auto& r : page.value().results) {
            if (r.id=="vidNavOnly") { nav=true; EXPECT(r.title=="Nav Title", "nav title"); }
            if (r.id=="vidNoAuthor") { noauth=true; EXPECT(r.title=="No Author No Dur", "no-author title"); EXPECT(r.subtitle.empty(), "no-author empty subtitle");
                auto* s=std::get_if<models::Song>(&r.payload); EXPECT(s && !s->durationSeconds.has_value(), "no duration"); EXPECT(s && !s->thumbnailUrl.has_value(), "no thumb"); }
        }
        EXPECT(nav && noauth, "found valid items");
    }
}

void test_playlist_item_duration_thumbnail() {
    std::cout<<"\n-- playlistItemRenderer duration/thumbnail variants --\n";
    std::string json = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[
      {"playlistItemRenderer":{"videoId":"vLong","title":{"simpleText":"Long Song"},"longBylineText":{"runs":[{"text":"Long Artist"}]},"lengthText":{"runs":[{"text":"1:02:03"}]},"thumbnail":{"musicThumbnailRenderer":{"thumbnail":{"thumbnails":[{"url":"https://example.com/long.jpg"}]}}}}},
      {"playlistItemRenderer":{"videoId":"vBadDur","title":{"runs":[{"text":"Bad Dur"}]},"shortBylineText":{"runs":[{"text":"A"}]},"lengthText":{"runs":[{"text":"live"}]},"thumbnailRenderer":{"musicThumbnailRenderer":{"thumbnail":{"thumbnails":[{"url":"https://example.com/live.jpg"}]}}}}}
    ]}}]}}}}]}}})json";
    auto page = youtube::YouTubeClient::parseLibraryPage(json);
    EXPECT(page.isOk(), "duration variants parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==2, "2 items");
        for (auto& r : page.value().results) {
            if (r.id=="vLong") { auto* s=std::get_if<models::Song>(&r.payload); EXPECT(s && s->durationSeconds.has_value() && *s->durationSeconds==3723, "1:02:03 -> 3723"); EXPECT(r.subtitle=="Long Artist", "longByline author"); EXPECT(s && s->thumbnailUrl.has_value(), "thumb via thumbnail key"); }
            if (r.id=="vBadDur") { auto* s=std::get_if<models::Song>(&r.payload); EXPECT(s && !s->durationSeconds.has_value(), "bad duration -> nullopt"); }
        }
    }
}

void test_playlist_mixed_renderers_items() {
    std::cout<<"\n-- mixed playlistRenderer + playlistItemRenderer --\n";
    std::string json = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[
      {"gridRenderer":{"items":[{"playlistRenderer":{"playlistId":"PLmix1","title":{"runs":[{"text":"My Mix"}]},"shortBylineText":{"runs":[{"text":"Me"}]},"videoCount":12}}]}},
      {"musicPlaylistShelfRenderer":{"contents":[
        {"playlistItemRenderer":{"videoId":"t1","title":{"runs":[{"text":"Track One"}]},"shortBylineText":{"runs":[{"text":"A1"}]},"lengthText":{"runs":[{"text":"2:30"}]}}},
        {"playlistItemRenderer":{"videoId":"t2","title":{"runs":[{"text":"Track Two"}]},"shortBylineText":{"runs":[{"text":"A2"}]},"lengthText":{"runs":[{"text":"3:00"}]}}}
      ]}}
    ]}}}}]}}})json";
    auto page = youtube::YouTubeClient::parsePlaylistsPage(json);
    EXPECT(page.isOk(), "mixed parse ok");
    if (page.isOk()) {
        EXPECT(page.value().results.size()==3, "1 playlist + 2 tracks");
        bool pl=false,tr1=false,tr2=false;
        for (auto& r : page.value().results) {
            if (r.id=="PLmix1") { pl=true; EXPECT(r.type==models::SearchResultType::Playlist, "playlist type kept"); }
            if (r.id=="t1") { tr1=true; EXPECT(r.type==models::SearchResultType::Song, "track type Song"); EXPECT(r.title=="Track One", "track title"); }
            if (r.id=="t2") tr2=true;
        }
        EXPECT(pl && tr1 && tr2, "found all 3");
    }
}

void test_playlist_item_continuation() {
    std::cout<<"\n-- playlistItemRenderer continuation --\n";
    std::string p1 = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[
      {"playlistItemRenderer":{"videoId":"c1","title":{"runs":[{"text":"First"}]},"shortBylineText":{"runs":[{"text":"A"}]}}}
    ],"continuations":[{"nextContinuationData":{"continuation":"plItemTok"}}]}}]}}}}]}}})json";
    std::string p2 = R"json({"continuationContents":{"musicPlaylistShelfContinuation":{"contents":[
      {"playlistItemRenderer":{"videoId":"c2","title":{"runs":[{"text":"Second"}]},"shortBylineText":{"runs":[{"text":"B"}]}}}
    ],"continuations":[]}}})json";
    auto r1 = youtube::YouTubeClient::parsePlaylistsPage(p1);
    auto r2 = youtube::YouTubeClient::parsePlaylistsPage(p2);
    EXPECT(r1.isOk() && r2.isOk(), "both pages parse");
    if (r1.isOk() && r2.isOk()) {
        EXPECT(r1.value().results.size()==1 && r1.value().results[0].id=="c1", "page1 item");
        EXPECT(r1.value().continuationToken.has_value() && *r1.value().continuationToken=="plItemTok", "page1 token");
        EXPECT(r2.value().results.size()==1 && r2.value().results[0].id=="c2", "page2 item");
        EXPECT(!r2.value().continuationToken.has_value(), "page2 no token");
    }
}

void test_playlist_item_malformed_empty() {
    std::cout<<"\n-- playlistItemRenderer malformed/empty --\n";
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage("").isErr(), "empty body err");
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage("{bad").isErr(), "malformed err");
    std::string emptyShelf = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[]}}]}}}}]}}})json";
    auto r = youtube::YouTubeClient::parsePlaylistsPage(emptyShelf);
    EXPECT(r.isOk() && r.value().results.empty(), "empty shelf -> 0 results");
    std::string unknown = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicPlaylistShelfRenderer":{"contents":[{"weirdRenderer":{"x":1}}]}}]}}}}]}}})json";
    auto r2 = youtube::YouTubeClient::parsePlaylistsPage(unknown);
    EXPECT(r2.isOk() && r2.value().results.empty(), "unknown renderer ignored");
    std::string apiErr = R"json({"error":{"code":403,"message":"Denied"}})json";
    EXPECT(youtube::YouTubeClient::parsePlaylistsPage(apiErr).isErr(), "api error");
    std::string flatMock = R"json({"results":[{"type":"song","id":"s1","title":"T","subtitle":"A"}]})json";
    auto rf = youtube::YouTubeClient::parsePlaylistsPage(flatMock);
    EXPECT(rf.isOk() && rf.value().results.size()==1, "flat mock fallback kept");
}

void test_player_request_body() {
    std::cout<<"\n-- player request body --\n";
    youtube::InnertubeConfig cfg;
    cfg.apiKey="TESTKEY"; cfg.clientName="WEB_REMIX"; cfg.clientVersion="1.20240702.01.00";
    std::string body = youtube::buildPlayerBody(cfg, "vid123");
    EXPECT(body.find("\"videoId\":\"vid123\"")!=std::string::npos, "videoId in body");
    EXPECT(body.find("\"clientName\":\"WEB_REMIX\"")!=std::string::npos, "client context");
    EXPECT(body.find("racyCheckOk")!=std::string::npos, "racyCheckOk");
    EXPECT(body.find("TESTKEY")==std::string::npos, "apiKey not in body");
    std::string withPl = youtube::buildPlayerBody(cfg, "vid123", std::string("PLabc"));
    EXPECT(withPl.find("\"playlistId\":\"PLabc\"")!=std::string::npos, "playlistId in body");
    std::string url = youtube::buildInnertubeUrl(cfg, "player");
    EXPECT(url.find("/youtubei/v1/player")!=std::string::npos, "player endpoint url");
    EXPECT(url.find("key=TESTKEY")!=std::string::npos, "key in url");
}

void test_player_playable_audio() {
    std::cout<<"\n-- player playable audio --\n";
    std::string json = R"json({"playabilityStatus":{"status":"OK","playableInEmbed":true},"videoDetails":{"videoId":"vid123","title":"Test Song","lengthSeconds":"202","author":"Artist"},"streamingData":{"formats":[{"itag":18,"url":"https://example.com/v18.mp4","mimeType":"video/mp4; codecs=\"avc1.42001E, mp4a.40.2\"","bitrate":500000}],"adaptiveFormats":[{"itag":140,"url":"https://example.com/a140.m4a","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"","bitrate":128000,"audioSampleRate":"44100","audioChannels":2,"contentLength":"3250000","approxDurationMs":"202000","audioQuality":"AUDIO_QUALITY_MEDIUM"}]}})json";
    auto r = youtube::YouTubeClient::parsePlayerResponse("vid123", json);
    EXPECT(r.isOk(), "playable parse ok");
    if (r.isOk()) {
        auto& res = r.value();
        EXPECT(res.isPlayable(), "isPlayable");
        EXPECT(res.videoId=="vid123", "videoId kept");
        EXPECT(res.audioStreams.size()==1, "only audio-only kept (video skipped)");
        EXPECT(res.hasPlayableStream(), "hasPlayableStream");
        auto& s = *res.selectedStream;
        EXPECT(s.itag==140, "selected itag 140");
        EXPECT(s.url=="https://example.com/a140.m4a", "selected url");
        EXPECT(s.mimeType.find("audio/mp4")!=std::string::npos, "mime audio/mp4");
        EXPECT(s.codecs=="mp4a.40.2", "codecs extracted");
        EXPECT(s.bitrate==128000, "bitrate");
        EXPECT(s.sampleRateHz==44100, "sample rate (string number)");
        EXPECT(s.channels==2, "channels");
        EXPECT(s.contentLengthBytes==3250000, "content length");
        EXPECT(s.approxDurationMs==202000, "approx duration");
        EXPECT(res.durationMs.has_value() && *res.durationMs==202000, "duration from videoDetails");
    }
}

void test_player_adaptive_selection() {
    std::cout<<"\n-- player adaptive selection --\n";
    // opus 160k vs mp4a 128k vs opus 48k vs cipher-only (skipped) vs video (skipped)
    std::string json = R"json({"playabilityStatus":{"status":"OK"},"videoDetails":{"videoId":"v","lengthSeconds":"180"},"streamingData":{"adaptiveFormats":[
      {"itag":249,"url":"https://example.com/a249.webm","mimeType":"audio/webm; codecs=\"opus\"","bitrate":50000,"audioSampleRate":"48000","audioChannels":2,"audioQuality":"AUDIO_QUALITY_LOW"},
      {"itag":140,"url":"https://example.com/a140.m4a","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"","bitrate":128000,"audioSampleRate":"44100","audioChannels":2,"audioQuality":"AUDIO_QUALITY_MEDIUM"},
      {"itag":251,"url":"https://example.com/a251.webm","mimeType":"audio/webm; codecs=\"opus\"","bitrate":160000,"audioSampleRate":"48000","audioChannels":2,"audioQuality":"AUDIO_QUALITY_HIGH"},
      {"itag":250,"signatureCipher":"s=abc&sp=sig&url=https://example.com/a250.webm","mimeType":"audio/webm; codecs=\"opus\"","bitrate":70000},
      {"itag":137,"url":"https://example.com/v137.mp4","mimeType":"video/mp4; codecs=\"avc1.640028\"","bitrate":4000000}
    ]}})json";
    auto r = youtube::YouTubeClient::parsePlayerResponse("v", json);
    EXPECT(r.isOk(), "adaptive parse ok");
    if (r.isOk()) {
        auto& res = r.value();
        EXPECT(res.audioStreams.size()==3, "3 usable (cipher + video skipped)");
        EXPECT(res.selectedStream.has_value() && res.selectedStream->itag==251, "opus 251 selected (codec rank, then bitrate)");
        // Same-codec bitrate preference via direct selection call
        std::vector<models::AudioStream> two = {
            {249, "https://example.com/lo", "audio/webm; codecs=\"opus\"", "opus", 50000, 48000, 2, 0, 0, "", true},
            {251, "https://example.com/hi", "audio/webm; codecs=\"opus\"", "opus", 160000, 48000, 2, 0, 0, "", true},
        };
        auto best = youtube::YouTubeClient::selectBestAudioStream(two);
        EXPECT(best.has_value() && best->itag==251, "higher bitrate wins within codec");
        // Same bitrate, codec tiebreak
        std::vector<models::AudioStream> tie = {
            {140, "https://example.com/aac", "audio/mp4; codecs=\"mp4a.40.2\"", "mp4a.40.2", 128000, 44100, 2, 0, 0, "", true},
            {251, "https://example.com/opus", "audio/webm; codecs=\"opus\"", "opus", 128000, 48000, 2, 0, 0, "", true},
        };
        auto bestTie = youtube::YouTubeClient::selectBestAudioStream(tie);
        EXPECT(bestTie.has_value() && bestTie->itag==251, "opus wins codec tiebreak");
        EXPECT(!youtube::YouTubeClient::selectBestAudioStream({}).has_value(), "empty -> nullopt");
    }
}

void test_player_missing_formats() {
    std::cout<<"\n-- player missing formats --\n";
    // OK status but no streamingData at all
    std::string noSd = R"json({"playabilityStatus":{"status":"OK"},"videoDetails":{"videoId":"v","lengthSeconds":"100"}})json";
    auto r1 = youtube::YouTubeClient::parsePlayerResponse("v", noSd);
    EXPECT(r1.isErr() && r1.error().kind==youtube::ErrorKind::Parse, "OK without streams -> Parse error");
    // OK status, only video formats
    std::string videoOnly = R"json({"playabilityStatus":{"status":"OK"},"streamingData":{"formats":[{"itag":18,"url":"https://example.com/v.mp4","mimeType":"video/mp4; codecs=\"avc1\"","bitrate":500000}]}})json";
    auto r2 = youtube::YouTubeClient::parsePlayerResponse("v", videoOnly);
    EXPECT(r2.isErr(), "video-only -> error");
    // OK status, only cipher streams (no decipher support)
    std::string cipherOnly = R"json({"playabilityStatus":{"status":"OK"},"streamingData":{"adaptiveFormats":[{"itag":251,"signatureCipher":"s=x","mimeType":"audio/webm; codecs=\"opus\"","bitrate":160000}]}})json";
    auto r3 = youtube::YouTubeClient::parsePlayerResponse("v", cipherOnly);
    EXPECT(r3.isErr(), "cipher-only -> error");
}

void test_player_unavailable_private() {
    std::cout<<"\n-- player unavailable/private --\n";
    // Deleted / blocked
    std::string err = R"json({"playabilityStatus":{"status":"ERROR","reason":"Video unavailable"}})json";
    auto r1 = youtube::YouTubeClient::parsePlayerResponse("v", err);
    EXPECT(r1.isOk(), "ERROR status still parses (data, not malformed)");
    if (r1.isOk()) {
        EXPECT(!r1.value().isPlayable(), "not playable");
        EXPECT(r1.value().playability==models::Playability::Unavailable, "Unavailable");
        EXPECT(r1.value().playabilityReason=="Video unavailable", "reason kept");
        EXPECT(!r1.value().hasPlayableStream(), "no stream");
    }
    // Private -> LOGIN_REQUIRED
    std::string priv = R"json({"playabilityStatus":{"status":"LOGIN_REQUIRED","reason":"This is a private video. Please sign in to verify that this is you."}})json";
    auto r2 = youtube::YouTubeClient::parsePlayerResponse("v", priv);
    EXPECT(r2.isOk() && r2.value().playability==models::Playability::LoginRequired, "private -> LoginRequired");
    // Age-gated via messages[]
    std::string age = R"json({"playabilityStatus":{"status":"LOGIN_REQUIRED","messages":["This video may be inappropriate for some users. Sign in to confirm your age."]}})json";
    auto r3 = youtube::YouTubeClient::parsePlayerResponse("v", age);
    EXPECT(r3.isOk() && r3.value().playability==models::Playability::AgeRestricted, "age gate -> AgeRestricted");
    // UNPLAYABLE blocked
    std::string blocked = R"json({"playabilityStatus":{"status":"UNPLAYABLE","reason":"Video blocked in your country"}})json";
    auto r4 = youtube::YouTubeClient::parsePlayerResponse("v", blocked);
    EXPECT(r4.isOk() && r4.value().playability==models::Playability::Unavailable, "blocked -> Unavailable");
    // resolvePlayback maps to user-facing errors
    struct PrivMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest&) override {
            return youtube::HttpResponse{200, R"({"playabilityStatus":{"status":"LOGIN_REQUIRED","reason":"Private video"}})", {}, ""};
        }
    };
    auto cPriv = youtube::YouTubeClient(std::make_unique<PrivMock>());
    auto rp = cPriv.resolvePlayback("v");
    EXPECT(rp.isErr() && rp.error().kind==youtube::ErrorKind::Auth, "resolve private -> Auth");
    struct GoneMock : public youtube::IHttpClient {
        youtube::HttpResponse execute(const youtube::HttpRequest&) override {
            return youtube::HttpResponse{200, R"({"playabilityStatus":{"status":"ERROR","reason":"Video unavailable"}})", {}, ""};
        }
    };
    auto cGone = youtube::YouTubeClient(std::make_unique<GoneMock>());
    auto rg = cGone.resolvePlayback("v");
    EXPECT(rg.isErr() && rg.error().kind==youtube::ErrorKind::NotFound, "resolve unavailable -> NotFound");
    EXPECT(rp.error().message.find("Private video")!=std::string::npos, "reason surfaced, no secrets");
}

void test_player_malformed() {
    std::cout<<"\n-- player malformed --\n";
    EXPECT(youtube::YouTubeClient::parsePlayerResponse("v", "").isErr(), "empty body");
    EXPECT(youtube::YouTubeClient::parsePlayerResponse("v", "{bad").isErr(), "malformed json");
    EXPECT(youtube::YouTubeClient::parsePlayerResponse("v", R"({"foo":1})").isErr(), "missing playabilityStatus");
    EXPECT(youtube::YouTubeClient::parsePlayerResponse("v", R"({"playabilityStatus":{}})").isErr(), "missing status");
    EXPECT(youtube::YouTubeClient::parsePlayerResponse("v", R"({"error":{"code":403,"message":"Denied"}})").isErr(), "api error field");
    auto cEmpty = youtube::YouTubeClient(std::make_unique<youtube::MockHttpClient>());
    EXPECT(cEmpty.resolvePlayback("").isErr(), "empty videoId");
}

void test_player_http_errors() {
    std::cout<<"\n-- player http errors --\n";
    auto http401 = std::make_unique<youtube::MockHttpClient>();
    http401->cannedResponse = youtube::HttpResponse{401, "", {}, ""};
    auto c401 = youtube::YouTubeClient(std::move(http401));
    EXPECT(c401.resolvePlayback("v").isErr(), "401 -> err");
    auto http500 = std::make_unique<youtube::MockHttpClient>();
    http500->cannedResponse = youtube::HttpResponse{500, "", {}, ""};
    auto c500 = youtube::YouTubeClient(std::move(http500));
    auto r500 = c500.resolvePlayback("v");
    EXPECT(r500.isErr() && r500.error().kind==youtube::ErrorKind::Network, "500 -> Network");
    auto http429 = std::make_unique<youtube::MockHttpClient>();
    http429->cannedResponse = youtube::HttpResponse{429, "", {}, ""};
    auto c429 = youtube::YouTubeClient(std::move(http429));
    EXPECT(c429.resolvePlayback("v").error().kind==youtube::ErrorKind::RateLimited, "429 -> RateLimited");
}

void test_resolve_playback_e2e() {
    std::cout<<"\n-- resolve playback e2e --\n";
    struct CapMock : public youtube::IHttpClient {
        youtube::HttpRequest lastReq;
        youtube::HttpResponse execute(const youtube::HttpRequest& req) override {
            lastReq = req;
            return youtube::HttpResponse{200, R"({"responseContext":{"visitorData":"VD123"},"playabilityStatus":{"status":"OK"},"videoDetails":{"videoId":"vid9","lengthSeconds":"200"},"streamingData":{"adaptiveFormats":[{"itag":251,"url":"https://example.com/a.webm","mimeType":"audio/webm; codecs=\"opus\"","bitrate":160000,"audioSampleRate":"48000","audioChannels":2}]}})", {}, ""};
        }
    };
    auto cap = std::make_unique<CapMock>();
    auto* raw = cap.get();
    youtube::InnertubeConfig cfg; cfg.apiKey="K123"; cfg.clientName="WEB_REMIX"; cfg.clientVersion="1.20240702.01.00";
    auto client = youtube::YouTubeClient(std::move(cap));
    client.setInnertubeConfig(cfg);
    client.setAuthHeaderProvider([]()->std::optional<std::string>{ return std::string("Bearer TokABC"); });
    auto r = client.resolvePlayback("vid9");
    EXPECT(r.isOk() && r.value().hasPlayableStream(), "e2e playable");
    EXPECT(raw->lastReq.url.find("/youtubei/v1/player")!=std::string::npos, "player endpoint");
    EXPECT(raw->lastReq.url.find("key=K123")!=std::string::npos, "key in url");
    EXPECT(raw->lastReq.body.find("\"videoId\":\"vid9\"")!=std::string::npos, "videoId in body");
    EXPECT(raw->lastReq.headers.count("Authorization") && raw->lastReq.headers.at("Authorization")=="Bearer TokABC", "auth header");
    EXPECT(raw->lastReq.headers.count("X-Goog-Api-Key"), "api key header");
    EXPECT(raw->lastReq.body.find("TokABC")==std::string::npos, "token not in body");
    EXPECT(raw->lastReq.body.find("K123")==std::string::npos, "api key not in body");
    EXPECT(client.innertubeConfig().visitorData=="VD123", "visitorData captured");
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
    test_all_filters();
    test_innertube_auth_context_hardening();
    test_library_browse_nested();
    test_playlists_browse_nested();
    test_history_browse_nested();
    test_browse_malformed();
    test_browse_mixed_renderers_and_continuation();
    test_history_twoRow_and_empty_shelf();
    test_playlist_item_renderer_normal();
    test_playlist_item_missing_fields();
    test_playlist_item_duration_thumbnail();
    test_playlist_mixed_renderers_items();
    test_playlist_item_continuation();
    test_playlist_item_malformed_empty();
    test_player_request_body();
    test_player_playable_audio();
    test_player_adaptive_selection();
    test_player_missing_formats();
    test_player_unavailable_private();
    test_player_malformed();
    test_player_http_errors();
    test_resolve_playback_e2e();
    std::cout<<"\n=== YouTube InnerTube Tests: "<<passed<<" passed, "<<failed<<" failed ===\n";
    return failed==0?0:1;
}
