#pragma once

#include <memory>
#include <string>

// UI Terminal is a thin facade over platform::ITerminal per AGENTS.md.
// All Win32 (GetConsoleScreenBufferInfo, ENABLE_VIRTUAL_TERMINAL_PROCESSING) lives in platform/terminal.
namespace myytm::platform { class ITerminal; }

namespace myytm::ui {

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    void init();
    void restore();

    void clear() const;
    void hideCursor() const;
    void showCursor() const;
    void moveTo(int row, int col) const;
    void write(const std::string& text) const;
    void writeLine(const std::string& text) const;

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

    void refreshSize();

private:
    std::unique_ptr<platform::ITerminal> impl_;
};

} // namespace myytm::ui
