#include "ui/input.h"

#ifdef _WIN32
#include <conio.h>
#endif
#include <cstdio>

namespace myytm::ui {

Key readKey()
{
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 224) {
        int ext = _getch();
        Key k;
        switch (ext) {
            case 72: k.code = KeyCode::ArrowUp; break;
            case 80: k.code = KeyCode::ArrowDown; break;
            case 75: k.code = KeyCode::ArrowLeft; break;
            case 77: k.code = KeyCode::ArrowRight; break;
            default: k.code = KeyCode::Unknown; break;
        }
        return k;
    }

    Key k;
    if (c == 27) { // ESC
        k.code = KeyCode::Escape;
        return k;
    }
    if (c == 13) {
        k.code = KeyCode::Enter;
        return k;
    }
    if (c == 8 || c == 127) {
        k.code = KeyCode::Backspace;
        return k;
    }
    if (c == 9) {
        k.code = KeyCode::Tab;
        return k;
    }
    // Ctrl combinations: 1..26
    if (c >= 1 && c <= 26) {
        k.code = KeyCode::Char;
        k.ch = static_cast<char>('a' + c - 1);
        k.ctrl = true;
        return k;
    }
    k.code = KeyCode::Char;
    k.ch = static_cast<char>(c);
    return k;
#else
    int c = std::getchar();
    Key k;
    if (c == EOF) return k;
    if (c == 27) { k.code = KeyCode::Escape; return k; }
    if (c == 10 || c == 13) { k.code = KeyCode::Enter; return k; }
    if (c == 127) { k.code = KeyCode::Backspace; return k; }
    k.code = KeyCode::Char;
    k.ch = static_cast<char>(c);
    return k;
#endif
}

} // namespace myytm::ui
