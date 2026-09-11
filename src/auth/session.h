#pragma once

#include "models/user_account.h"

#include <chrono>
#include <optional>
#include <string>

namespace myytm::auth {

// Opaque session — never logs or exposes raw tokens.
// Tokens are held only in memory and persisted via ICredentialStore (DPAPI).
struct Session {
    models::UserAccount account;
    std::string accessToken;  // opaque, never logged
    std::string refreshToken; // opaque, never logged
    std::chrono::system_clock::time_point expiresAt{};

    [[nodiscard]] bool isExpired() const noexcept
    {
        return std::chrono::system_clock::now() >= expiresAt;
    }
    [[nodiscard]] bool isExpiringSoon(std::chrono::seconds window) const noexcept
    {
        return std::chrono::system_clock::now() + window >= expiresAt;
    }
    [[nodiscard]] bool hasTokens() const noexcept
    {
        return !accessToken.empty() && !refreshToken.empty();
    }
    // Safe view for UI — never returns tokens
    [[nodiscard]] std::string safeLabel() const
    {
        if (!account.displayName.empty()) return account.displayName;
        if (!account.email.empty()) return account.email;
        return account.id;
    }
    void clearSecrets() noexcept
    {
        accessToken.clear();
        refreshToken.clear();
    }
};

} // namespace myytm::auth
