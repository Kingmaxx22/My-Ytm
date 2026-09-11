#include "ui/help_screen.h"
#include "ui/keymap.h"
#include "ui/renderer.h"

namespace myytm::ui {

HelpScreen::HelpScreen()
{
    lines_ = {
        "Navigation        j:down  k:up  h:back  l/Enter:open  gg:top  G:bottom  Ctrl-d/u:half-page",
        "Search            /:search  n:next  N:prev  Esc:exit/clear  Enter:play",
        "Playback          Space:play/pause  ]:next  [:prev  H:seek -10s  L:seek +10s  +/-:volume",
        "Queue             a:add next  A:append  d:remove  c:clear  6:Queue  r:reload",
        "Application       1:Home  2:Search  3:Library  4:Playlists  5:History  0:Account  6:Queue  ?:help  q:quit/back",
        "Account           o:launch browser  O:sign out  r:refresh  D:demo sign-in (DPAPI)",
        "Config            Volume/theme/cache in %APPDATA%\\MyYtm\\config.json  — not secrets",
        "Logging           %APPDATA%\\MyYtm\\myytm.log  (never logs tokens/cookies)",
        "Errors            \"Unable to connect\" / \"Rate limited\" / \"Auth failed\" — no secrets in messages",
        "",
        "Context-sensitive help will show keys for the active screen.",
        "Press q or Esc to return. j/k or arrows to scroll. gg/G top/bottom.",
    };
}

void HelpScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();
    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);

    int firstRow = 4;
    int visible = h - 5;
    if (visible < 1) visible = 1;

    size_t top = 0;
    if (selected_ >= static_cast<size_t>(visible)) top = selected_ - static_cast<size_t>(visible) + 1;
    if (top + static_cast<size_t>(visible) > lines_.size()) {
        if (lines_.size() > static_cast<size_t>(visible)) top = lines_.size() - static_cast<size_t>(visible);
        else top = 0;
    }

    for (int i = 0; i < visible; ++i) {
        size_t idx = top + static_cast<size_t>(i);
        if (idx >= lines_.size()) break;
        int row = firstRow + i;
        std::string prefix = (idx == selected_) ? "> " : "  ";
        std::string line = prefix + lines_[idx];
        if (static_cast<int>(line.size()) > w - 1) line = line.substr(0, static_cast<size_t>(w - 1));
        if (idx == selected_)
            r.text(row, 1, Renderer::reverse(line + std::string(static_cast<size_t>(w - 1 - line.size()), ' ')));
        else
            r.text(row, 1, line);
    }
    r.text(h - 1, 1, Renderer::dim("j/k:scroll  g g/G:top/bottom  q/Esc:back"));
}

bool HelpScreen::handleKey(const Key& key)
{
    if (keymap::isDown(key)) { moveDown(); return true; }
    if (keymap::isUp(key)) { moveUp(); return true; }
    if (keymap::isLast(key)) { goBottom(); pendingG_ = false; return true; }
    if (key.code == KeyCode::Char && key.ch == 'g') {
        if (pendingG_) { goTop(); pendingG_ = false; return true; }
        pendingG_ = true;
        return true;
    }
    pendingG_ = false;
    return false;
}

void HelpScreen::moveDown() { if (selected_ + 1 < lines_.size()) ++selected_; }
void HelpScreen::moveUp() { if (selected_ > 0) --selected_; }
void HelpScreen::goTop() { selected_ = 0; }
void HelpScreen::goBottom() { if (!lines_.empty()) selected_ = lines_.size() - 1; }

} // namespace myytm::ui
