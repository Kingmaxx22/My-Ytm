#include "ui/history_screen.h"
#include "ui/keymap.h"
#include "ui/renderer.h"
#include "player/queue.h"
#include "player/player.h"
#include "youtube/youtube_client.h"

namespace myytm::ui {

void HistoryScreen::onEnter() { reload(); }

void HistoryScreen::reload()
{
    loading_ = true;
    continuationToken_.reset();
    if (!client_) { items_.clear(); statusMsg_ = "Client not configured."; loading_ = false; return; }
    auto res = client_->getHistoryPage(std::nullopt);
    loading_ = false;
    if (res.isOk()) {
        items_ = res.value().results;
        continuationToken_ = res.value().continuationToken;
        selected_ = 0;
        if (continuationToken_) statusMsg_ = "Press > for more (" + std::to_string(items_.size()) + " recent)";
        else statusMsg_.clear();
    } else { items_.clear(); continuationToken_.reset(); statusMsg_ = res.error().message; }
}

bool HistoryScreen::loadNextPage()
{
    if (!client_ || !continuationToken_) return false;
    loading_ = true;
    auto res = client_->getHistoryPage(*continuationToken_);
    loading_ = false;
    if (res.isErr()) { statusMsg_ = res.error().message; return false; }
    auto& page = res.value();
    size_t before = items_.size();
    items_.insert(items_.end(), page.results.begin(), page.results.end());
    continuationToken_ = page.continuationToken;
    if (page.results.empty() && !continuationToken_) statusMsg_ = "No more results";
    else if (continuationToken_) statusMsg_ = "Loaded " + std::to_string(page.results.size()) + " more (total " + std::to_string(items_.size()) + ") — > for more";
    else statusMsg_.clear();
    if (selected_ < before) {} else selected_ = before;
    return true;
}

void HistoryScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();
    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);
    if (loading_) r.text(3, 1, Renderer::dim("Loading history..."));
    else if (!statusMsg_.empty()) r.text(3, 1, Renderer::dim(statusMsg_));
    else if (continuationToken_) r.text(3, 1, Renderer::dim(std::to_string(items_.size()) + " recent — a/A queue, Enter replay, r reload, > for more"));
    else r.text(3, 1, Renderer::dim(std::to_string(items_.size()) + " recent — a/A queue, Enter replay, r reload"));
    int firstRow = 5;
    int visible = h - 6;
    if (visible < 1) visible = 1;
    if (items_.empty() && !loading_) r.text(firstRow, 1, Renderer::dim("No history. Press r to reload."));
    else {
        size_t top = 0;
        if (selected_ >= static_cast<size_t>(visible)) top = selected_ - static_cast<size_t>(visible) + 1;
        if (top + static_cast<size_t>(visible) > items_.size()) {
            if (items_.size() > static_cast<size_t>(visible)) top = items_.size() - static_cast<size_t>(visible);
            else top = 0;
        }
        for (int i = 0; i < visible; ++i) {
            size_t idx = top + static_cast<size_t>(i);
            if (idx >= items_.size()) break;
            int row = firstRow + i;
            const auto& it = items_[idx];
            std::string prefix = (idx == selected_) ? "> " : "  ";
            std::string line = prefix + "[" + it.typeLabel() + "] " + it.display();
            if (static_cast<int>(line.size()) > w - 1) line = line.substr(0, static_cast<size_t>(w - 1));
            if (idx == selected_) { line += std::string(static_cast<size_t>(w - 1 - line.size()), ' '); r.text(row, 1, Renderer::reverse(line)); }
            else r.text(row, 1, line);
        }
    }
    if (continuationToken_ && !loading_ && statusMsg_.empty()) r.text(h - 2, 1, Renderer::dim("More available — press > to load next page"));
    std::string footer = std::string("j/k:move  gg/G:top/bottom  a/A:queue  l/Enter:replay  r:reload") + (continuationToken_ ? "  >:more" : "");
    r.text(h - 1, 1, Renderer::dim(footer));
}

bool HistoryScreen::handleKey(const Key& key)
{
    if (key.code == KeyCode::Char && key.ch == 'r') { reload(); return true; }
    if (key.code == KeyCode::Char && key.ch == '>') {
        if (continuationToken_) { loadNextPage(); return true; }
        statusMsg_ = "No more results";
        return true;
    }
    if (keymap::isDown(key)) { moveDown(); return true; }
    if (keymap::isUp(key)) { moveUp(); return true; }
    if (keymap::isLast(key)) { goBottom(); pendingG_ = false; return true; }
    if (key.code == KeyCode::Char && key.ch == 'g') { if (pendingG_) { goTop(); pendingG_ = false; return true; } pendingG_ = true; return true; }
    pendingG_ = false;
    if (keymap::isAddToQueue(key) || keymap::isAddToEnd(key)) {
        if (items_.empty() || selected_ >= items_.size() || !queue_) return true;
        auto qi = models::QueueItem::fromSearchResult(items_[selected_]);
        if (keymap::isAddToQueue(key)) queue_->addNext(qi); else queue_->addToEnd(qi);
        statusMsg_ = (keymap::isAddToQueue(key) ? "Added next: " : "Queued: ") + items_[selected_].display();
        return true;
    }
    if (keymap::isOpen(key)) {
        if (items_.empty() || selected_ >= items_.size() || !queue_ || !player_) return true;
        auto qi = models::QueueItem::fromSearchResult(items_[selected_]);
        auto before = queue_->currentIndex();
        queue_->addNext(qi);
        size_t target = before ? *before + 1 : 0;
        if (target >= queue_->size()) target = queue_->size() - 1;
        queue_->setCurrent(target);
        player_->play();
        statusMsg_ = "Replaying: " + items_[selected_].display();
        return true;
    }
    return false;
}

void HistoryScreen::moveDown() { if (!items_.empty() && selected_ + 1 < items_.size()) ++selected_; }
void HistoryScreen::moveUp() { if (selected_ > 0) --selected_; }
void HistoryScreen::goTop() { selected_ = 0; }
void HistoryScreen::goBottom() { if (!items_.empty()) selected_ = items_.size() - 1; }

} // namespace myytm::ui
