#include "ui/search_screen.h"
#include "ui/keymap.h"
#include "ui/renderer.h"
#include "player/queue.h"
#include "player/player.h"
#include "youtube/youtube_client.h"

#include <algorithm>
#include <cctype>

namespace myytm::ui {

SearchScreen::SearchScreen() : SearchScreen(nullptr) {}

SearchScreen::SearchScreen(std::shared_ptr<youtube::YouTubeClient> client) : client_(std::move(client))
{
    // In-memory catalog — fallback when YouTubeClient unavailable/offline.
    catalog_ = {
        models::SearchResult::fromSong({"s1", "Blinding Lights", "The Weeknd", "After Hours", 200}),
        models::SearchResult::fromSong({"s2", "Bohemian Rhapsody", "Queen", "A Night at the Opera", 354}),
        models::SearchResult::fromSong({"s3", "Hotel California", "Eagles", "Hotel California", 390}),
        models::SearchResult::fromSong({"s4", "Shape of You", "Ed Sheeran", "÷", 233}),
        models::SearchResult::fromSong({"s5", "Viva La Vida", "Coldplay", "Viva La Vida", 242}),
        models::SearchResult::fromArtist({"a1", "The Weeknd"}),
        models::SearchResult::fromArtist({"a2", "Queen"}),
        models::SearchResult::fromArtist({"a3", "Coldplay"}),
        models::SearchResult::fromAlbum({"al1", "After Hours", "The Weeknd", 2020}),
        models::SearchResult::fromAlbum({"al2", "A Night at the Opera", "Queen", 1975}),
        models::SearchResult::fromAlbum({"al3", "Parachutes", "Coldplay", 2000}),
        models::SearchResult::fromPlaylist({"p1", "Liked Songs", "You", 128}),
        models::SearchResult::fromPlaylist({"p2", "Chill Mix", "YouTube Music", 50}),
        models::SearchResult::fromPlaylist({"p3", "Road Trip", "You", 34}),
    };
    results_ = catalog_;
}

std::string SearchScreen::toLower(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

bool SearchScreen::containsCi(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) return true;
    std::string h = toLower(haystack);
    std::string n = toLower(needle);
    return h.find(n) != std::string::npos;
}

void SearchScreen::setQuery(std::string q)
{
    query_ = std::move(q);
}

void SearchScreen::executeSearch()
{
    committedQuery_ = query_;
    continuationToken_.reset();

    // Prefer YouTubeClient when wired (UI never builds HTTP directly)
    if (client_) {
        auto res = client_->searchPage(query_, std::nullopt, filter_);
        if (res.isOk()) {
            results_ = res.value().results;
            continuationToken_ = res.value().continuationToken;
            selected_ = 0;
            pendingG_ = false;
            if (results_.empty()) statusMsg_ = "No results for \"" + committedQuery_ + "\"" + (filter_ != youtube::SearchFilter::All ? " [" + youtube::searchFilterLabel(filter_) + "]" : "");
            else if (continuationToken_) statusMsg_ = "Press > for more (" + std::to_string(results_.size()) + " results)" + (filter_ != youtube::SearchFilter::All ? " [" + youtube::searchFilterLabel(filter_) + "]" : "");
            else statusMsg_.clear();
            return;
        }
        // Error path: user-facing message, no secrets, keep previous results fallback
        statusMsg_ = res.error().message;
        if (res.error().kind == youtube::ErrorKind::Parse) {
            results_.clear();
        } else if (results_.empty()) {
            results_ = catalog_;
        }
        selected_ = 0;
        pendingG_ = false;
        continuationToken_.reset();
        return;
    }

    auto matchesFilter = [&](const models::SearchResult& r){
        if (filter_ == youtube::SearchFilter::All) return true;
        if (filter_ == youtube::SearchFilter::Songs) return r.type == models::SearchResultType::Song;
        if (filter_ == youtube::SearchFilter::Videos) return r.type == models::SearchResultType::Song;
        if (filter_ == youtube::SearchFilter::Albums) return r.type == models::SearchResultType::Album;
        if (filter_ == youtube::SearchFilter::Artists) return r.type == models::SearchResultType::Artist;
        if (filter_ == youtube::SearchFilter::Playlists) return r.type == models::SearchResultType::Playlist;
        return true;
    };
    if (query_.empty()) {
        results_.clear();
        for (auto& r : catalog_) if (matchesFilter(r)) results_.push_back(r);
    } else {
        results_.clear();
        for (auto& r : catalog_) {
            if (matchesFilter(r) && (containsCi(r.title, query_) || containsCi(r.subtitle, query_) || containsCi(r.typeLabel(), query_))) {
                results_.push_back(r);
            }
        }
    }
    selected_ = 0;
    pendingG_ = false;
    continuationToken_.reset();
    if (results_.empty()) {
        statusMsg_ = "No results for \"" + committedQuery_ + "\"";
    } else {
        statusMsg_.clear();
    }
}

void SearchScreen::toggleSongsFilter() {
    filter_ = (filter_ == youtube::SearchFilter::Songs ? youtube::SearchFilter::All : youtube::SearchFilter::Songs);
    if (!committedQuery_.empty()) {
        query_ = committedQuery_;
        continuationToken_.reset();
        executeSearch();
    } else if (!query_.empty()) {
        committedQuery_ = query_;
        continuationToken_.reset();
        executeSearch();
    } else {
        statusMsg_ = std::string("Filter: ") + youtube::searchFilterLabel(filter_);
    }
}

bool SearchScreen::loadNextPage() {
    if (!client_ || !continuationToken_) return false;
    auto res = client_->searchPage(committedQuery_, *continuationToken_, filter_);
    if (res.isErr()) {
        statusMsg_ = res.error().message;
        return false;
    }
    // Append without corrupting existing results
    auto& page = res.value();
    size_t before = results_.size();
    results_.insert(results_.end(), page.results.begin(), page.results.end());
    continuationToken_ = page.continuationToken;
    if (page.results.empty() && !continuationToken_) statusMsg_ = "No more results";
    else if (continuationToken_) statusMsg_ = "Loaded " + std::to_string(page.results.size()) + " more (total " + std::to_string(results_.size()) + ") — > for more";
    else statusMsg_.clear();
    // Keep selection at previous end
    if (selected_ < before) { /* keep */ } else selected_ = before;
    return true;
}

void SearchScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();

    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);

    // Search bar at row 3
    std::string bar;
    if (mode_ == Mode::Input) {
        bar = "/" + query_ + "_";
        r.text(3, 1, Renderer::reverse(bar + std::string(static_cast<size_t>(std::max(0, w - 1 - static_cast<int>(bar.size()))), ' ')));
    } else {
        std::string filterLabel = (filter_ != youtube::SearchFilter::All ? " [" + youtube::searchFilterLabel(filter_) + "]" : "");
        if (committedQuery_.empty() && query_.empty()) {
            bar = "Press / to search" + filterLabel + "  •  " + std::to_string(results_.size()) + " items  •  n/N next/prev  Esc exit";
        } else {
            bar = "Query: \"" + committedQuery_ + "\"" + filterLabel + "  (" + std::to_string(results_.size()) + " results)  —  / to refine";
        }
        if (static_cast<int>(bar.size()) > w - 1) bar = bar.substr(0, static_cast<size_t>(w - 1));
        r.text(3, 1, Renderer::dim(bar));
    }

    int firstRow = 5;
    int visible = h - 6; // header(2) + bar + gap + status
    if (visible < 1) visible = 1;

    if (results_.empty()) {
        std::string msg = statusMsg_.empty() ? "No results." : statusMsg_;
        r.text(firstRow, 1, Renderer::dim(msg));
        r.text(firstRow + 1, 1, Renderer::dim("Try a different search term.  / to search again."));
    } else {
        size_t top = 0;
        if (selected_ >= static_cast<size_t>(visible)) top = selected_ - static_cast<size_t>(visible) + 1;
        if (top + static_cast<size_t>(visible) > results_.size()) {
            if (results_.size() > static_cast<size_t>(visible)) top = results_.size() - static_cast<size_t>(visible);
            else top = 0;
        }
        for (int i = 0; i < visible; ++i) {
            size_t idx = top + static_cast<size_t>(i);
            if (idx >= results_.size()) break;
            int row = firstRow + i;
            const auto& res = results_[idx];
            std::string prefix = (idx == selected_) ? "> " : "  ";
            std::string line = prefix + "[" + res.typeLabel() + "] " + res.display();
            if (static_cast<int>(line.size()) > w - 1) line = line.substr(0, static_cast<size_t>(w - 1));
            if (idx == selected_) {
                line += std::string(static_cast<size_t>(w - 1 - line.size()), ' ');
                r.text(row, 1, Renderer::reverse(line));
            } else {
                r.text(row, 1, line);
            }
        }
    }

    if (!statusMsg_.empty()) {
        r.text(h - 2, 1, Renderer::dim(statusMsg_));
    } else if (continuationToken_) {
        r.text(h - 2, 1, Renderer::dim("More results available — press > to load next page"));
    }
    std::string footer;
    if (mode_ == Mode::Input) footer = "Enter:search  Esc:cancel  Backspace:delete";
    else footer = std::string("j/k:move  /:search  n/N:next/prev  gg/G:top/bottom  l:play  a/A:queue") + (filter_ != youtube::SearchFilter::All ? "  f:filter*" : "  f:filter") + (continuationToken_ ? "  >:more" : "") + "  Esc:clear";
    r.text(h - 1, 1, Renderer::dim(footer));
}

bool SearchScreen::handleKey(const Key& key)
{
    if (mode_ == Mode::Input) {
        if (key.code == KeyCode::Enter) {
            mode_ = Mode::Browsing;
            executeSearch();
            return true;
        }
        if (key.code == KeyCode::Escape) {
            mode_ = Mode::Browsing;
            query_.clear();
            // keep previous results/committed query
            return true;
        }
        if (key.code == KeyCode::Backspace) {
            if (!query_.empty()) query_.pop_back();
            return true;
        }
        if (key.code == KeyCode::Char && !key.ctrl) {
            // Basic printable filter
            if (key.ch >= 32 && key.ch < 127) {
                query_.push_back(key.ch);
            }
            return true;
        }
        return true; // consume all in input mode
    }

    // Browsing mode
    if (keymap::isSearch(key)) {
        mode_ = Mode::Input;
        query_.clear();
        return true;
    }
    if (key.code == KeyCode::Escape) {
        // Clear search on Esc in browsing — return to full catalog
        if (!committedQuery_.empty() || !query_.empty()) {
            query_.clear();
            committedQuery_.clear();
            results_ = catalog_;
            selected_ = 0;
            statusMsg_.clear();
            pendingG_ = false;
            return true;
        }
        pendingG_ = false;
        return false;
    }
    if (keymap::isDown(key) || keymap::isNextResult(key)) { moveDown(); return true; }
    if (keymap::isUp(key) || keymap::isPrevResult(key)) { moveUp(); return true; }
    if (keymap::isHalfPageDown(key)) { halfPageDown(); return true; }
    if (keymap::isHalfPageUp(key)) { halfPageUp(); return true; }
    if (keymap::isLast(key)) { goBottom(); pendingG_ = false; return true; }
    if (key.code == KeyCode::Char && key.ch == 'g') {
        if (pendingG_) { goTop(); pendingG_ = false; return true; }
        pendingG_ = true;
        return true;
    }
    if (keymap::isAddToQueue(key) || keymap::isAddToEnd(key)) {
        if (!results_.empty() && selected_ < results_.size() && queue_) {
            auto qi = models::QueueItem::fromSearchResult(results_[selected_]);
            if (keymap::isAddToQueue(key)) queue_->addNext(qi);
            else queue_->addToEnd(qi);
            statusMsg_ = std::string(keymap::isAddToQueue(key) ? "Added next: " : "Queued: ") + results_[selected_].display();
            return true;
        }
        statusMsg_ = "Queue not available.";
        return true;
    }
    if (keymap::isOpen(key)) {
        if (!results_.empty() && selected_ < results_.size()) {
            if (queue_ && player_) {
                auto qi = models::QueueItem::fromSearchResult(results_[selected_]);
                auto before = queue_->currentIndex();
                queue_->addNext(qi);
                size_t target = before ? *before + 1 : 0;
                if (target >= queue_->size()) target = queue_->size() - 1;
                queue_->setCurrent(target);
                player_->play();
                statusMsg_ = "Playing: " + results_[selected_].display();
            } else {
                statusMsg_ = "Selected: " + results_[selected_].display();
            }
            return true;
        }
    }
    if (key.code == KeyCode::Char && key.ch == '>') {
        if (continuationToken_) {
            if (loadNextPage()) return true;
            statusMsg_ = "Failed to load more";
            return true;
        }
        statusMsg_ = "No more results";
        return true;
    }
    if (key.code == KeyCode::Char && (key.ch == 'f' || key.ch == 'F')) {
        toggleSongsFilter();
        return true;
    }
    pendingG_ = false;
    return false;
}

void SearchScreen::moveDown() { if (!results_.empty() && selected_ + 1 < results_.size()) ++selected_; }
void SearchScreen::moveUp() { if (selected_ > 0) --selected_; }
void SearchScreen::goTop() { selected_ = 0; }
void SearchScreen::goBottom() { if (!results_.empty()) selected_ = results_.size() - 1; }
void SearchScreen::halfPageDown() { if (!results_.empty()) selected_ = std::min(results_.size() - 1, selected_ + 8); }
void SearchScreen::halfPageUp() { if (selected_ >= 8) selected_ -= 8; else selected_ = 0; }

} // namespace myytm::ui
