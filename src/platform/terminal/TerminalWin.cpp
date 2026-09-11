#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "platform/terminal/ITerminal.h"
#include <cstdio>

namespace myytm::platform {

class TerminalWin final : public ITerminal {
public:
    TerminalWin() = default;
    ~TerminalWin() override { if (initialized_) restore(); }

    void init() override {
        if (initialized_) return;
        enableVirtualTerminal();
        refreshSize();
        initialized_ = true;
        hideCursor();
        clear();
    }
    void restore() override {
        if (!initialized_) return;
        showCursor();
        clear();
        disableVirtualTerminal();
        initialized_ = false;
    }
    void clear() const override {
        std::fputs("\x1b[2J\x1b[H", stdout);
        std::fflush(stdout);
    }
    void hideCursor() const override { std::fputs("\x1b[?25l", stdout); std::fflush(stdout); }
    void showCursor() const override { std::fputs("\x1b[?25h", stdout); std::fflush(stdout); }
    void moveTo(int row, int col) const override { std::printf("\x1b[%d;%dH", row, col); }
    void write(const std::string& text) const override { std::fputs(text.c_str(), stdout); }
    void writeLine(const std::string& text) const override { std::fputs(text.c_str(), stdout); std::fputs("\n", stdout); }
    int width() const noexcept override { return width_; }
    int height() const noexcept override { return height_; }
    void refreshSize() override {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info{};
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hOut, &info)) {
            width_ = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
            height_ = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
            if (width_<=0) width_=80;
            if (height_<=0) height_=24;
        }
#else
        width_=80; height_=24;
#endif
    }
private:
    void enableVirtualTerminal() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        savedOutHandle_ = hOut; savedInHandle_ = hIn;
        DWORD outMode=0;
        if (GetConsoleMode(hOut, &outMode)) { savedOutMode_=outMode; outMode|=ENABLE_VIRTUAL_TERMINAL_PROCESSING|DISABLE_NEWLINE_AUTO_RETURN; SetConsoleMode(hOut,outMode); }
        DWORD inMode=0;
        if (GetConsoleMode(hIn,&inMode)) { savedInMode_=inMode; }
#endif
    }
    void disableVirtualTerminal() {
#ifdef _WIN32
        if (savedOutHandle_) SetConsoleMode(savedOutHandle_, savedOutMode_);
        if (savedInHandle_) SetConsoleMode(savedInHandle_, savedInMode_);
#endif
    }
    bool initialized_=false;
    int width_=80, height_=24;
#ifdef _WIN32
    void* savedOutHandle_=nullptr; unsigned long savedOutMode_=0;
    void* savedInHandle_=nullptr; unsigned long savedInMode_=0;
#endif
};

std::unique_ptr<ITerminal> makeTerminal() {
    return std::make_unique<TerminalWin>();
}

} // namespace myytm::platform
