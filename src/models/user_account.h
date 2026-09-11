#pragma once

#include <optional>
#include <string>

namespace myytm::models {

struct UserAccount {
    std::string id;
    std::string email;
    std::string displayName;
    std::optional<std::string> avatarUrl;

    [[nodiscard]] bool isValid() const noexcept { return !id.empty(); }
};

} // namespace myytm::models
