#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace myytm::models {

struct Song {
    std::string id;
    std::string title;
    std::string artist;
    std::string album;
    std::optional<int> durationSeconds; // nullable
    std::optional<std::string> thumbnailUrl;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !id.empty() && !title.empty();
    }

    [[nodiscard]] std::string display() const
    {
        std::string s = title;
        if (!artist.empty()) s += " — " + artist;
        if (!album.empty()) s += " [" + album + "]";
        return s;
    }
};

} // namespace myytm::models
