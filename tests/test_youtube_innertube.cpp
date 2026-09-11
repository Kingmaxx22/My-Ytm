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
    cfg.clientVersion = "1.20240101.00.00";
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

int main(){
    test_flat_mock_still_works();
    test_innerTube_search_nested();
    test_twoRow_and_artist_album_playlist();
    test_library_grid();
    test_missing_optional_fields();
    test_malformed_json();
    test_continuation_parsing();
    test_pagination_append();
    test_api_error_response();
    test_empty_search_response();
    test_innertube_url_and_body();
    std::cout<<"\n=== YouTube InnerTube Tests: "<<passed<<" passed, "<<failed<<" failed ===\n";
    return failed==0?0:1;
}
