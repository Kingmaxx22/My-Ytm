#pragma once

#include <string_view>

namespace myytm::auth {

enum class AuthState {
    SignedOut,
    Authenticating, // browser flow in progress
    SignedIn,
    Error,
};

inline std::string_view toString(AuthState s) noexcept
{
    switch (s) {
        case AuthState::SignedOut: return "SignedOut";
        case AuthState::Authenticating: return "Authenticating";
        case AuthState::SignedIn: return "SignedIn";
        case AuthState::Error: return "Error";
    }
    return "?";
}

} // namespace myytm::auth
