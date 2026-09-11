#pragma once

#include "ui/terminal.h"

#include <string>
#include <string_view>

namespace myytm::ui {

class Renderer {
public:
    explicit Renderer(Terminal& terminal) : term_(terminal) {}

    void clear() const { term_.clear(); }

    void text(int row, int col, std::string_view s) const
    {
        term_.moveTo(row, col);
        term_.write(std::string(s));
    }

    void hline(int row, int col, int len, char ch = '-') const
    {
        term_.moveTo(row, col);
        term_.write(std::string(static_cast<size_t>(len), ch));
    }

    void box(int row, int col, int w, int h) const
    {
        if (w < 2 || h < 2) return;
        text(row, col, "+" + std::string(static_cast<size_t>(w - 2), '-') + "+");
        for (int r = 1; r < h - 1; ++r) {
            text(row + r, col, "|");
            text(row + r, col + w - 1, "|");
        }
        text(row + h - 1, col, "+" + std::string(static_cast<size_t>(w - 2), '-') + "+");
    }

    // ANSI helpers
    static std::string bold(std::string_view s) { return "\x1b[1m" + std::string(s) + "\x1b[22m"; }
    static std::string dim(std::string_view s) { return "\x1b[2m" + std::string(s) + "\x1b[22m"; }
    static std::string reverse(std::string_view s) { return "\x1b[7m" + std::string(s) + "\x1b[27m"; }

    Terminal& terminal() { return term_; }
    const Terminal& terminal() const { return term_; }

private:
    Terminal& term_;
};

} // namespace myytm::ui
