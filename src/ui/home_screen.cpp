#include "ui/home_screen.h"
#include "ui/keymap.h"
#include "ui/renderer.h"

namespace myytm::ui {

HomeScreen::HomeScreen()
{
    items_ = {
        "Welcome to My-Ytm",
        "",
        "A Windows-first YouTube Music TUI — vim-style, keyboard-driven.",
        "",
        "Press ? for help   1:Home  2:Search  3:Library  4:Playlists  5:History  q:Quit",
        "",
        "Navigation: j/k move  h back  l open  gg top  G bottom  Ctrl-d/u half-page",
        "Search:     / search  n/N next/prev  Esc exit",
        "Playback:   Space play/pause  ]/[ next/prev  H/L seek  +/- volume",
        "Queue:      a add  A append  d remove  c clear",
        "",
        "— Phase 2: UI foundation active. Search/YouTube/Player coming next. —",
    };
}

void HomeScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();

    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);

    int firstRow = 4;
    int visible = h - 5; // reserve header(2) + margin + status bar
    if (visible < 1) visible = 1;

    // Keep selected visible
    size_t top = 0;
    if (selected_ >= static_cast<size_t>(visible)) {
        top = selected_ - static_cast<size_t>(visible) + 1;
    }
    if (top + static_cast<size_t>(visible) > items_.size()) {
        if (items_.size() > static_cast<size_t>(visible))
            top = items_.size() - static_cast<size_t>(visible);
        else
            top = 0;
    }

    for (int i = 0; i < visible; ++i) {
        size_t idx = top + static_cast<size_t>(i);
        if (idx >= items_.size()) break;
        int row = firstRow + i;
        std::string prefix = (idx == selected_) ? "> " : "  ";
        std::string line = prefix + items_[idx];
        if (static_cast<int>(line.size()) > w - 1) line = line.substr(0, static_cast<size_t>(w - 1));
        if (idx == selected_)
            r.text(row, 1, Renderer::reverse(line + std::string(static_cast<size_t>(w - 1 - line.size()), ' ')));
        else
            r.text(row, 1, line);
    }

    r.text(h - 1, 1, Renderer::dim("j/k:move  gg/G:top/bottom  Ctrl-d/u:half-page  ?:help  q:quit"));
}

bool HomeScreen::handleKey(const Key& key)
{
    if (keymap::isDown(key)) { moveDown(); return true; }
    if (keymap::isUp(key)) { moveUp(); return true; }
    if (keymap::isHalfPageDown(key)) { halfPageDown(); return true; }
    if (keymap::isHalfPageUp(key)) { halfPageUp(); return true; }
    if (keymap::isLast(key)) { goBottom(); pendingG_ = false; return true; }

    // gg handling
    if (key.code == KeyCode::Char && key.ch == 'g') {
        if (pendingG_) { goTop(); pendingG_ = false; return true; }
        pendingG_ = true;
        return true;
    }
    pendingG_ = false;
    return false;
}

void HomeScreen::moveDown()
{
    if (items_.empty()) return;
    if (selected_ + 1 < items_.size()) ++selected_;
}

void HomeScreen::moveUp()
{
    if (selected_ > 0) --selected_;
}

void HomeScreen::goTop()
{
    selected_ = 0;
}

void HomeScreen::goBottom()
{
    if (!items_.empty()) selected_ = items_.size() - 1;
}

void HomeScreen::halfPageDown()
{
    if (items_.empty()) return;
    selected_ = std::min(items_.size() - 1, selected_ + 8);
}

void HomeScreen::halfPageUp()
{
    if (selected_ >= 8) selected_ -= 8;
    else selected_ = 0;
}

} // namespace myytm::ui
