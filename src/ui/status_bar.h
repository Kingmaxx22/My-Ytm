#pragma once

#include "ui/renderer.h"

#include <string_view>

namespace myytm::ui {

class StatusBar {
public:
    void setMessage(std::string_view msg) { message_ = std::string(msg); }
    void clearMessage() { message_.clear(); }
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    void render(const Renderer& r, std::string_view screenName) const
    {
        const int row = r.terminal().height();
        const int w = r.terminal().width();
        // Bottom line: reverse video
        std::string left = std::string(screenName);
        std::string right = message_.empty() ? "?:Help  q:Quit  1:Home 2:Search 3:Lib 4:Playlists 5:History" : message_;
        std::string bar;
        bar.reserve(static_cast<size_t>(w));
        bar += " " + left + " ";
        int filler = w - static_cast<int>(bar.size()) - static_cast<int>(right.size()) - 1;
        if (filler > 0) bar += std::string(static_cast<size_t>(filler), ' ');
        bar += " " + right;

        if (static_cast<int>(bar.size()) > w) bar = bar.substr(0, static_cast<size_t>(w));
        else if (static_cast<int>(bar.size()) < w) bar += std::string(static_cast<size_t>(w - bar.size()), ' ');

        r.text(row, 1, Renderer::reverse(bar));
    }

private:
    std::string message_;
};

} // namespace myytm::ui
