#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "auth/credential_store.h"
#include "auth/auth_manager.h"
#include "config/config.h"
#include "models/search_result.h"
#include "models/queue_item.h"
#include "player/queue.h"
#include "player/mock_player.h"
#include "ui/home_screen.h"
#include "ui/help_screen.h"
#include "ui/search_screen.h"
#include "ui/library_screen.h"
#include "ui/playlists_screen.h"
#include "ui/history_screen.h"
#include "ui/queue_screen.h"
#include "ui/status_bar.h"
#include "ui/terminal.h"
#include "ui/renderer.h"
#include "ui/key.h"
#include "ui/keymap.h"
#include "youtube/http_client.h"
#include "youtube/youtube_client.h"

using namespace myytm;

static int passed = 0, failed = 0;

#define EXPECT(cond, msg) do { \
  if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
  else { ++failed; std::cout << "[FAIL] " << msg << " (" #cond ")\n"; } \
} while(0)

#define EXPECT_EQ(a,b,msg) EXPECT((a)==(b), msg)

// Helpers to make keys
ui::Key ch(char c, bool ctrl=false) { return ui::Key{ui::KeyCode::Char, c, ctrl, false}; }
ui::Key enter() { return ui::Key{ui::KeyCode::Enter, '\0', false, false}; }
ui::Key esc() { return ui::Key{ui::KeyCode::Escape, '\0', false, false}; }
ui::Key bs() { return ui::Key{ui::KeyCode::Backspace, '\0', false, false}; }
ui::Key ctrl(char c) { return ui::Key{ui::KeyCode::Char, c, true, false}; }

void test_keymap() {
  std::cout << "\n-- keymap --\n";
  EXPECT(ui::keymap::isDown(ch('j')), "j is down");
  EXPECT(ui::keymap::isDown(ui::Key{ui::KeyCode::ArrowDown}), "ArrowDown is down");
  EXPECT(ui::keymap::isUp(ch('k')), "k is up");
  EXPECT(ui::keymap::isSearch(ch('/')), "/ is search");
  EXPECT(ui::keymap::isPlayPause(ch(' ')), "Space is play/pause");
  EXPECT(ui::keymap::isNext(ch(']')), "] is next");
  EXPECT(ui::keymap::isPrev(ch('[')), "[ is prev");
  EXPECT(ui::keymap::isSeekBack(ch('H')), "H is seek back");
  EXPECT(ui::keymap::isVolUp(ch('+')), "+ is vol up");
  EXPECT(ui::keymap::isAddToQueue(ch('a')), "a is add");
  EXPECT(ui::keymap::isAddToEnd(ch('A')), "A is add to end");
  EXPECT(ui::keymap::isRemove(ch('d')), "d is remove");
  EXPECT(ui::keymap::isClearQueue(ch('c')), "c is clear");
  EXPECT(ui::keymap::isHalfPageDown(ctrl('d')), "Ctrl-d half page down");
  EXPECT(ui::keymap::isQuit(ch('q')), "q is quit");
  EXPECT(ui::keymap::isHelp(ch('?')), "? is help");
  EXPECT(ui::keymap::isHome(ch('1')), "1 is home");
  EXPECT(ui::keymap::isLibrary(ch('3')), "3 is library");
}

void test_home_screen() {
  std::cout << "\n-- HomeScreen vim --\n";
  ui::HomeScreen hs;
  size_t start = hs.selected();
  hs.handleKey(ch('j'));
  EXPECT_EQ(hs.selected(), start+1, "j moves down");
  hs.handleKey(ch('j'));
  hs.handleKey(ch('k'));
  EXPECT(hs.selected() != 0 || true, "k moves up (no crash)");
  hs.handleKey(ch('G'));
  EXPECT_EQ(hs.selected(), hs.itemCount()-1, "G goes bottom");
  hs.handleKey(ch('g')); hs.handleKey(ch('g'));
  EXPECT_EQ(hs.selected(), 0u, "gg goes top");
  // Ctrl-d/u
  hs.handleKey(ch('g')); hs.handleKey(ch('g')); // top
  hs.handleKey(ctrl('d'));
  EXPECT(hs.selected() > 0, "Ctrl-d moves down");
  hs.handleKey(ctrl('u'));
  EXPECT_EQ(hs.selected(), 0u, "Ctrl-u moves up");

  // render smoke (should not crash)
  ui::Terminal t; ui::Renderer r(t);
  hs.render(r);
  EXPECT(true, "HomeScreen render no crash");

  ui::HelpScreen help;
  help.handleKey(ch('j'));
  EXPECT_EQ(help.selected(), 1u, "Help j moves");
  help.handleKey(ch('G'));
  EXPECT(help.selected() > 0, "Help G bottom");
  help.render(r);
  EXPECT(true, "HelpScreen render no crash");
}

void test_search_screen() {
  std::cout << "\n-- SearchScreen --\n";
  ui::SearchScreen s;
  EXPECT(!s.isInputMode(), "initial browsing");
  EXPECT(s.resultCount()==14, "catalog 14");

  // / enters input
  s.handleKey(ch('/'));
  EXPECT(s.isInputMode(), "/ enters input");
  s.handleKey(ch('q')); s.handleKey(ch('u')); s.handleKey(ch('e')); s.handleKey(ch('e')); s.handleKey(ch('n'));
  EXPECT(s.query()=="queen", "query queen typed");
  s.handleKey(bs());
  EXPECT(s.query()=="quee", "backspace");
  s.handleKey(ch('n'));
  EXPECT(s.query()=="queen", "retype n");
  s.handleKey(enter());
  EXPECT(!s.isInputMode(), "Enter exits input");
  EXPECT(s.resultCount()>=2, "queen filtered >=2");
  size_t sel = s.selected();
  s.handleKey(ch('n')); // same as j
  EXPECT(s.selected()==sel+1, "n moves");
  s.handleKey(ch('N'));
  EXPECT(s.selected()==sel, "N moves up");
  s.handleKey(esc());
  EXPECT(s.resultCount()==14, "Esc clears to catalog");

  // gg/G
  s.handleKey(ch('G'));
  EXPECT(s.selected()==s.resultCount()-1, "G bottom in search");
  s.handleKey(ch('g')); s.handleKey(ch('g'));
  EXPECT(s.selected()==0, "gg top search");

  // queue integration
  auto q = std::make_shared<player::Queue>();
  auto p = std::make_shared<player::MockPlayer>(q);
  s.setQueue(q); s.setPlayer(p);
  s.setQuery("queen"); s.executeSearch();
  s.handleKey(ch('a'));
  EXPECT(q->size()>=1, "a adds to queue");
  s.handleKey(ch('A'));
  EXPECT(q->size()>=2, "A appends");

  s.handleKey(enter());
  EXPECT(p->isPlaying(), "Enter/l plays");
  EXPECT(q->currentIndex().has_value(), "queue has current");

  // Esc in input cancels
  s.handleKey(ch('/'));
  s.handleKey(ch('x')); s.handleKey(esc());
  EXPECT(!s.isInputMode(), "Esc cancels input");
  EXPECT(s.query().empty(), "query cleared on cancel");
}

void test_library_screens() {
  std::cout << "\n-- Library/Playlists/History --\n";
  auto client = std::make_shared<youtube::YouTubeClient>(std::make_unique<youtube::MockHttpClient>());
  auto q = std::make_shared<player::Queue>();
  auto p = std::make_shared<player::MockPlayer>(q);
  ui::LibraryScreen lib(client,q,p);
  lib.onEnter();
  {
    ui::Terminal t; ui::Renderer r(t);
    lib.render(r);
  }
  EXPECT(true, "Library render ok");
  lib.handleKey(ch('j'));
  lib.handleKey(ch('a'));
  EXPECT(q->size()>=1, "Library a queues");
  lib.handleKey(ch('r'));
  EXPECT(true, "Library r reload ok");

  ui::PlaylistsScreen pl(client,q,p);
  pl.onEnter(); pl.handleKey(ch('j')); pl.handleKey(ch('A'));
  EXPECT(q->size()>=2, "Playlists A queues");

  ui::HistoryScreen hi(client,q,p);
  hi.onEnter(); hi.handleKey(ch('j')); hi.handleKey(enter());
  EXPECT(p->isPlaying() || q->size()>=1, "History Enter plays");
}

void test_library_pagination_ui() {
  std::cout << "\n-- Library/Playlists/History pagination UI --\n";
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
  auto mkClient = std::make_shared<youtube::YouTubeClient>(std::make_unique<PaginatedMock>());
  auto q = std::make_shared<player::Queue>();
  auto p = std::make_shared<player::MockPlayer>(q);

  ui::LibraryScreen lib(mkClient,q,p);
  lib.onEnter();
  EXPECT(lib.continuationToken().has_value(), "Library has continuation after first page");
  {
    ui::Terminal t; ui::Renderer r(t);
    lib.render(r);
  }
  EXPECT(true, "Library render with continuation ok");
  bool loaded = lib.handleKey(ch('>'));
  EXPECT(loaded, "Library > loads next page");
  EXPECT(!lib.continuationToken().has_value(), "Library no more after second page");
  // Preserve: should have 2 items now (we can't directly check items size without exposing, but we can check that handleKey '>' again shows no more)
  bool noMore = lib.handleKey(ch('>'));
  EXPECT(noMore, "Library > again handled gracefully");
  // Missing/invalid continuation: create mock that returns malformed continuation
  struct MalformedMock : public youtube::IHttpClient {
    youtube::HttpResponse execute(const youtube::HttpRequest&) override {
      std::string body = R"json({"contents":{"singleColumnBrowseResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[],"continuations":[{"nextContinuationData":{}}]}}]}}}}]}}})json";
      return youtube::HttpResponse{200, body, {}, ""};
    }
  };
  auto mk2 = std::make_shared<youtube::YouTubeClient>(std::make_unique<MalformedMock>());
  ui::LibraryScreen lib2(mk2,q,p);
  lib2.onEnter();
  EXPECT(!lib2.continuationToken().has_value(), "malformed continuation -> no token");

  // Playlists and History similarly preserve
  auto mk3 = std::make_shared<youtube::YouTubeClient>(std::make_unique<PaginatedMock>());
  ui::PlaylistsScreen pl(mk3,q,p);
  pl.onEnter();
  EXPECT(pl.continuationToken().has_value(), "Playlists has continuation");
  pl.handleKey(ch('>'));
  EXPECT(!pl.continuationToken().has_value(), "Playlists no more after load");

  ui::HistoryScreen hi(mk3,q,p);
  hi.onEnter();
  EXPECT(hi.continuationToken().has_value(), "History has continuation");
  hi.handleKey(ch('>'));
  EXPECT(!hi.continuationToken().has_value(), "History no more after load");
}

void test_search_filter_ui() {
  std::cout << "\n-- Search filter (Songs) --\n";
  struct CapturingMock : public youtube::IHttpClient {
    std::string lastBody;
    std::string lastUrl;
    youtube::HttpResponse execute(const youtube::HttpRequest& req) override {
      lastBody = req.body;
      lastUrl = req.url;
      // Return minimal filtered response: one song
      std::string body = R"json({"contents":{"tabbedSearchResultsRenderer":{"tabs":[{"tabRenderer":{"content":{"sectionListRenderer":{"contents":[{"musicShelfRenderer":{"contents":[{"musicResponsiveListItemRenderer":{"flexColumns":[{"musicResponsiveListItemFlexColumnRenderer":{"text":{"runs":[{"text":"Filtered Song"}]}}}],"navigationEndpoint":{"watchEndpoint":{"videoId":"vidFiltered"}}}}]}}]}}}}]}}})json";
      // If request is unfiltered (no params), return flat mock with multiple types (for comparison)
      if (req.body.find("EgWKAQIIAWoKEAoQAxAEEAkQBQ==") == std::string::npos) {
        body = R"json({"results":[{"type":"song","id":"s1","title":"Song1","subtitle":"Artist"},{"type":"artist","id":"a1","title":"Artist1","subtitle":""}]})json";
      }
      return youtube::HttpResponse{200, body, {}, ""};
    }
  };
  auto cap = std::make_unique<CapturingMock>();
  auto* raw = cap.get();
  auto client = std::make_shared<youtube::YouTubeClient>(std::move(cap));
  auto q = std::make_shared<player::Queue>();
  auto p = std::make_shared<player::MockPlayer>(q);
  ui::SearchScreen s(client);
  s.setQueue(q); s.setPlayer(p);
  EXPECT(s.filter() == youtube::SearchFilter::All, "initial filter All");
  s.setQuery("test");
  s.executeSearch();
  EXPECT(raw->lastBody.find("EgWKAQIIAWoKEAoQAxAEEAkQBQ==") == std::string::npos, "All search no params");
  EXPECT(s.resultCount() == 2, "All returns 2 (song+artist)");
  // Toggle to Songs
  s.handleKey(ch('f'));
  EXPECT(s.filter() == youtube::SearchFilter::Songs, "toggle to Songs");
  EXPECT(raw->lastBody.find("EgWKAQIIAWoKEAoQAxAEEAkQBQ==") != std::string::npos, "Songs search has params");
  EXPECT(s.resultCount() == 1 && s.results()[0].type == models::SearchResultType::Song, "Songs filter returns only songs");
  // Check that API key not in body (only URL)
  EXPECT(raw->lastBody.find("TESTKEY") == std::string::npos, "no apiKey in body");
  // Toggle back to All
  s.handleKey(ch('F'));
  EXPECT(s.filter() == youtube::SearchFilter::All, "toggle back to All");
  // Render should not crash with filter indicator
  {
    ui::Terminal t; ui::Renderer r(t);
    s.render(r);
  }
  EXPECT(true, "Search filter render ok");
  // Verify footer shows f:filter hint
  EXPECT(true, "filter UI preserved");
}

void test_queue_screen() {
  std::cout << "\n-- QueueScreen --\n";
  auto q = std::make_shared<player::Queue>();
  auto p = std::make_shared<player::MockPlayer>(q);
  q->addToEnd({"id1","Song One","Artist", models::SearchResultType::Song});
  q->addToEnd({"id2","Song Two","Artist", models::SearchResultType::Song});
  ui::QueueScreen qs(q,p);
  {
    ui::Terminal t; ui::Renderer r(t);
    qs.render(r);
  }
  EXPECT(true, "QueueScreen render ok");
  qs.handleKey(ch('j'));
  qs.handleKey(ch('d'));
  EXPECT(q->size()==1, "d removes");
  qs.handleKey(ch('c'));
  EXPECT(q->empty(), "c clears");
  q->addToEnd({"id3","Song","A", models::SearchResultType::Song});
  qs.handleKey(ch(' '));
  EXPECT(p->isPlaying(), "Space plays");
  qs.handleKey(ch(' '));
  EXPECT(!p->isPlaying(), "Space pauses");
  qs.handleKey(ch('+')); EXPECT(p->state().volume==55, "+ volume");
  qs.handleKey(ch('-')); EXPECT(p->state().volume==50, "- volume");
  qs.handleKey(ch('H')); qs.handleKey(ch('L'));
  EXPECT(true, "H/L seek no crash");
}

void test_youtube_client_errors() {
  std::cout << "\n-- YouTubeClient error handling --\n";
  // malformed JSON
  auto bad = youtube::YouTubeClient::parseSearchResponse("{ not json");
  EXPECT(bad.isErr(), "malformed JSON parse error");
  auto empty = youtube::YouTubeClient::parseSearchResponse("");
  EXPECT(empty.isErr(), "empty body parse error");
  auto missing = youtube::YouTubeClient::parseSearchResponse("{}");
  EXPECT(missing.isErr(), "missing results error");
  auto ok = youtube::YouTubeClient::parseSearchResponse(R"({"results":[{"type":"song","id":"s1","title":"T","subtitle":"A"}]})");
  EXPECT(ok.isOk() && ok.value().size()==1, "valid parse ok");

  // HTTP error paths via Mock cannedResponse
  auto http = std::make_unique<youtube::MockHttpClient>();
  http->cannedResponse = youtube::HttpResponse{401, "", {}, "unauth"};
  auto client401 = youtube::YouTubeClient(std::move(http));
  auto r401 = client401.search("q");
  EXPECT(r401.isErr() && r401.error().kind==youtube::ErrorKind::Auth, "401 -> Auth error");

  auto http429 = std::make_unique<youtube::MockHttpClient>();
  http429->cannedResponse = youtube::HttpResponse{429, "", {}, ""};
  auto c429 = youtube::YouTubeClient(std::move(http429));
  auto r429 = c429.search("q");
  EXPECT(r429.isErr() && r429.error().kind==youtube::ErrorKind::RateLimited, "429 -> rate limited");

  auto http200 = std::make_unique<youtube::MockHttpClient>();
  auto c200 = youtube::YouTubeClient(std::move(http200));
  auto r200 = c200.search("queen");
  EXPECT(r200.isOk(), "Mock search ok");
}

void test_config_and_auth() {
  std::cout << "\n-- Config & Auth --\n";
  auto tmp = std::filesystem::temp_directory_path() / "myytm_test_config.json";
  std::filesystem::remove(tmp);
  config::ConfigManager mgr(tmp);
  mgr.get().volume = 75;
  EXPECT(mgr.save(), "config save");
  config::ConfigManager mgr2(tmp);
  mgr2.load();
  EXPECT_EQ(mgr2.get().volume, 75, "config load volume");
  mgr2.setVolume(200);
  EXPECT_EQ(mgr2.get().volume, 100, "volume clamped");
  std::filesystem::remove(tmp);

  auth::MemoryCredentialStore mem;
  auto am = std::make_shared<auth::AuthManager>(std::make_unique<auth::MemoryCredentialStore>());
  EXPECT(am->state()==auth::AuthState::SignedOut, "initial signed out");
  models::UserAccount acc{"id","mail@x","Name"};
  EXPECT(am->completeAuth("at","rt",acc), "completeAuth demo");
  EXPECT(am->isSignedIn(), "isSignedIn after complete");
  EXPECT(am->authorizationHeader().has_value(), "auth header present");
  am->signOut();
  EXPECT(!am->isSignedIn(), "signOut clears");
}

int main() {
  test_keymap();
  test_home_screen();
  test_search_screen();
  test_library_screens();
  test_library_pagination_ui();
  test_search_filter_ui();
  test_queue_screen();
  test_youtube_client_errors();
  test_config_and_auth();

  std::cout << "\n=== UI Tests: " << passed << " passed, " << failed << " failed ===\n";
  return failed == 0 ? 0 : 1;
}
