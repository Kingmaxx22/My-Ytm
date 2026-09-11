#include "ui/queue_screen.h"
#include "ui/keymap.h"
#include "ui/renderer.h"

namespace myytm::ui {

void QueueScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();
    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);

    auto st = player_->state();
    std::string now = st.hasTrack ? st.currentTitle + (st.currentSubtitle.empty() ? "" : " — " + st.currentSubtitle) : "(no track)";
    std::string statusLine = std::string(models::toString(st.status)) + "  vol:" + std::to_string(st.volume) + "  " + st.timeLabel() + "  |  " + now;
    if (static_cast<int>(statusLine.size()) > w - 1) statusLine = statusLine.substr(0, static_cast<size_t>(w - 1));
    r.text(3, 1, Renderer::dim(statusLine));
    r.text(4, 1, Renderer::dim("Space:play/pause  ]/[ :next/prev  H/L:seek  +/-:vol  d:remove  c:clear  Enter:play selected"));

    if (!msg_.empty()) {
        r.text(5, 1, Renderer::reverse(msg_ + std::string(static_cast<size_t>(std::max(0, w - 1 - static_cast<int>(msg_.size()))), ' ')));
    }

    int firstRow = msg_.empty() ? 6 : 7;
    int visible = h - firstRow - 1;
    if (visible < 1) visible = 1;

    if (queue_->empty()) {
        r.text(firstRow, 1, Renderer::dim("Queue empty. Go to Search (2) and press a/A to add."));
        r.text(h - 1, 1, Renderer::dim("j/k:move  a/A:add  d:remove  c:clear"));
        return;
    }

    size_t top = 0;
    if (selected_ >= static_cast<size_t>(visible)) top = selected_ - static_cast<size_t>(visible) + 1;
    if (top + static_cast<size_t>(visible) > queue_->size()) {
        if (queue_->size() > static_cast<size_t>(visible)) top = queue_->size() - static_cast<size_t>(visible);
        else top = 0;
    }

    auto curIdx = queue_->currentIndex();

    for (int i = 0; i < visible; ++i) {
        size_t idx = top + static_cast<size_t>(i);
        if (idx >= queue_->size()) break;
        int row = firstRow + i;
        const auto* item = queue_->at(idx);
        std::string prefix = (idx == selected_) ? "> " : "  ";
        std::string marker = (curIdx && *curIdx == idx) ? (player_->isPlaying() ? "▶ " : "● ") : "  ";
        std::string line = prefix + marker + item->display();
        if (static_cast<int>(line.size()) > w - 1) line = line.substr(0, static_cast<size_t>(w - 1));
        if (idx == selected_) {
            line += std::string(static_cast<size_t>(w - 1 - line.size()), ' ');
            r.text(row, 1, Renderer::reverse(line));
        } else if (curIdx && *curIdx == idx) {
            r.text(row, 1, Renderer::bold(line));
        } else {
            r.text(row, 1, line);
        }
    }
    r.text(h - 1, 1, Renderer::dim("j/k:move  gg/G:top/bottom  Enter:l play  d/c:remove/clear  Space:play/pause"));
}

bool QueueScreen::handleKey(const Key& key)
{
    // Playback — also handled globally in App, but allow here
    if (keymap::isPlayPause(key)) {
        if (queue_->empty()) { msg_ = "Queue empty."; return true; }
        auto s = player_->state();
        if (s.status == models::PlaybackStatus::Playing) player_->pause();
        else player_->play();
        msg_.clear();
        return true;
    }
    if (keymap::isNext(key)) { player_->next(); msg_.clear(); return true; }
    if (keymap::isPrev(key)) { player_->previous(); msg_.clear(); return true; }
    if (keymap::isSeekBack(key)) { player_->seek(-10); return true; }
    if (keymap::isSeekForward(key)) { player_->seek(10); return true; }
    if (keymap::isVolUp(key)) { player_->volumeUp(); return true; }
    if (keymap::isVolDown(key)) { player_->volumeDown(); return true; }

    if (keymap::isDown(key)) { moveDown(); return true; }
    if (keymap::isUp(key)) { moveUp(); return true; }
    if (keymap::isLast(key)) { goBottom(); pendingG_ = false; return true; }
    if (key.code == KeyCode::Char && key.ch == 'g') {
        if (pendingG_) { goTop(); pendingG_ = false; return true; }
        pendingG_ = true;
        return true;
    }
    pendingG_ = false;

    if (keymap::isRemove(key)) {
        if (queue_->empty()) return true;
        queue_->removeAt(selected_);
        if (selected_ >= queue_->size() && selected_ > 0) --selected_;
        msg_ = "Removed.";
        return true;
    }
    if (keymap::isClearQueue(key)) {
        queue_->clear();
        player_->stop();
        selected_ = 0;
        msg_ = "Queue cleared.";
        return true;
    }
    if (keymap::isOpen(key)) {
        if (queue_->empty()) return true;
        queue_->setCurrent(selected_);
        player_->play();
        msg_.clear();
        return true;
    }
    return false;
}

void QueueScreen::moveDown() { if (!queue_->empty() && selected_ + 1 < queue_->size()) ++selected_; }
void QueueScreen::moveUp() { if (selected_ > 0) --selected_; }
void QueueScreen::goTop() { selected_ = 0; }
void QueueScreen::goBottom() { if (!queue_->empty()) selected_ = queue_->size() - 1; }

} // namespace myytm::ui
