#pragma once

#include <string>

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

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

    void refreshSize();

private:
    void enableVirtualTerminal();
    void disableVirtualTerminal();

    bool initialized_ = false;
    int width_ = 80;
    int height_ = 24;

#ifdef _WIN32
    void* savedOutHandle_ = nullptr;
    unsigned long savedOutMode_ = 0;
    void* savedInHandle_ = nullptr;
    unsigned long savedInMode_ = 0;
#endif
};

} // namespace myytm::ui
