#pragma once

#include <optional>
#include <string>

namespace myytm::ui {

enum class KeyCode {
    Char,
    Enter,
    Escape,
    Backspace,
    Tab,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Unknown,
};

struct Key {
    KeyCode code = KeyCode::Unknown;
    char ch = '\0'; // valid when code == Char
    bool ctrl = false;
    bool alt = false;

    [[nodiscard]] bool isChar(char c) const noexcept
    {
        return code == KeyCode::Char && ch == c;
    }

    [[nodiscard]] std::string toString() const
    {
        if (code == KeyCode::Char) {
            if (ctrl && ch)
                return std::string("C-") + ch;
            return std::string(1, ch);
        }
        switch (code) {
            case KeyCode::Enter: return "Enter";
            case KeyCode::Escape: return "Esc";
            case KeyCode::Backspace: return "BS";
            case KeyCode::Tab: return "Tab";
            case KeyCode::ArrowUp: return "Up";
            case KeyCode::ArrowDown: return "Down";
            case KeyCode::ArrowLeft: return "Left";
            case KeyCode::ArrowRight: return "Right";
            default: return "?";
        }
    }
};

} // namespace myytm::ui
