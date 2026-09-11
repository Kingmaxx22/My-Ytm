#pragma once

#include <optional>
#include <string>

namespace myytm::models {

struct Album {
    std::string id;
    std::string title;
    std::string artist;
    std::optional<int> year;
    std::optional<std::string> thumbnailUrl;

    [[nodiscard]] bool isValid() const noexcept { return !id.empty() && !title.empty(); }
};

} // namespace myytm::models
