#pragma once

#include <string>
#include <vector>

namespace myytm::models {

struct Playlist {
    std::string id;
    std::string title;
    std::string author;
    size_t trackCount = 0;

    [[nodiscard]] bool isValid() const noexcept { return !id.empty() && !title.empty(); }
};

} // namespace myytm::models
