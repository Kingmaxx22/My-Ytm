#pragma once

#include <optional>
#include <string>

namespace myytm::models {

struct Artist {
    std::string id;
    std::string name;
    std::optional<std::string> thumbnailUrl;

    [[nodiscard]] bool isValid() const noexcept { return !id.empty() && !name.empty(); }
};

} // namespace myytm::models
