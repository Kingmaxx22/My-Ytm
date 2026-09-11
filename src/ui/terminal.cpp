#include "ui/terminal.h"

#include <cstdio>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace myytm::ui {

Terminal::Terminal() = default;

Terminal::~Terminal()
{
    if (initialized_)
        restore();
}

void Terminal::init()
{
    if (initialized_)
        return;
    enableVirtualTerminal();
    refreshSize();
    initialized_ = true;
    hideCursor();
    clear();
}

void Terminal::restore()
{
    if (!initialized_)
        return;
    showCursor();
    clear();
    disableVirtualTerminal();
    initialized_ = false;
}

void Terminal::clear() const
{
    // ANSI clear screen + home
    std::fputs("\x1b[2J\x1b[H", stdout);
    std::fflush(stdout);
}

void Terminal::hideCursor() const
{
    std::fputs("\x1b[?25l", stdout);
    std::fflush(stdout);
}

void Terminal::showCursor() const
{
    std::fputs("\x1b[?25h", stdout);
    std::fflush(stdout);
}

void Terminal::moveTo(int row, int col) const
{
    std::printf("\x1b[%d;%dH", row, col);
}

void Terminal::write(const std::string& text) const
{
    std::fputs(text.c_str(), stdout);
}

void Terminal::writeLine(const std::string& text) const
{
    std::fputs(text.c_str(), stdout);
    std::fputs("\n", stdout);
}

void Terminal::refreshSize()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &info)) {
        width_ = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
        height_ = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
        if (width_ <= 0) width_ = 80;
        if (height_ <= 0) height_ = 24;
    }
#else
    width_ = 80;
    height_ = 24;
#endif
}

void Terminal::enableVirtualTerminal()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    savedOutHandle_ = hOut;
    savedInHandle_ = hIn;

    DWORD outMode = 0;
    if (GetConsoleMode(hOut, &outMode)) {
        savedOutMode_ = outMode;
        outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        SetConsoleMode(hOut, outMode);
    }

    DWORD inMode = 0;
    if (GetConsoleMode(hIn, &inMode)) {
        savedInMode_ = inMode;
        // Keep basic line input off for _getch to work; no changes needed here.
    }
#endif
}

void Terminal::disableVirtualTerminal()
{
#ifdef _WIN32
    if (savedOutHandle_) {
        SetConsoleMode(savedOutHandle_, savedOutMode_);
    }
    if (savedInHandle_) {
        SetConsoleMode(savedInHandle_, savedInMode_);
    }
#endif
}

} // namespace myytm::ui
