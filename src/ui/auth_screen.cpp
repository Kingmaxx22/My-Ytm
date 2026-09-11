#include "ui/auth_screen.h"
#include "ui/renderer.h"

namespace myytm::ui {

void AuthScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();
    r.text(1, 1, Renderer::bold(title()));
    r.hline(2, 1, w);

    auto state = auth_->state();
    std::string stateStr = std::string(auth::toString(state));
    std::string label = auth_->session() ? auth_->session()->safeLabel() : "(not signed in)";

    r.text(4, 1, "State:   " + stateStr);
    r.text(5, 1, "Account: " + label);
    if (!auth_->lastError().empty()) {
        r.text(6, 1, Renderer::dim(std::string("Error: ") + std::string(auth_->lastError())));
    } else {
        r.text(6, 1, Renderer::dim("No errors."));
    }

    r.text(8, 1, Renderer::dim("Browser-based auth only — never enter your Google password into My-Ytm."));
    r.text(9, 1, "  o : launch browser to sign in");
    r.text(10, 1, "  O : sign out (clears secure storage)");
    r.text(11, 1, "  r : refresh session");
    r.text(12, 1, "  D : demo sign-in (no real credentials, for offline demo)");

    if (!msg_.empty()) {
        r.text(14, 1, Renderer::reverse(msg_ + std::string(static_cast<size_t>(std::max(0, w - 1 - static_cast<int>(msg_.size()))), ' ')));
    }

    r.text(h - 1, 1, Renderer::dim("q/Esc:back  1:Home  ?:Help"));
}

bool AuthScreen::handleKey(const Key& key)
{
    if (key.code == KeyCode::Char && key.ch == 'o') {
        bool ok = auth_->beginBrowserAuth();
        msg_ = ok ? "Browser launched. Complete sign-in in browser, then press D for demo." : std::string(auth_->lastError());
        return true;
    }
    if (key.code == KeyCode::Char && key.ch == 'O') {
        auth_->signOut();
        msg_ = "Signed out. Secrets cleared from secure storage.";
        return true;
    }
    if (key.code == KeyCode::Char && key.ch == 'r') {
        bool ok = auth_->refresh();
        msg_ = ok ? "Session refreshed." : std::string(auth_->lastError());
        return true;
    }
    if (key.code == KeyCode::Char && key.ch == 'D') {
        // Demo — no real Google password, opaque demo tokens
        models::UserAccount acc{"demo-id", "demo@myytm.local", "Demo User"};
        bool ok = auth_->completeAuth("demo-access-token", "demo-refresh-token", acc, 3600);
        msg_ = ok ? "Demo signed in as Demo User (tokens stored via DPAPI)." : std::string(auth_->lastError());
        return true;
    }
    return false;
}

} // namespace myytm::ui
