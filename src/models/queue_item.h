#pragma once

#include "models/search_result.h"

#include <string>

namespace myytm::models {

struct QueueItem {
    std::string id;
    std::string title;
    std::string subtitle;
    SearchResultType type = SearchResultType::Song;

    [[nodiscard]] static QueueItem fromSearchResult(const SearchResult& r)
    {
        return {r.id, r.title, r.subtitle, r.type};
    }

    [[nodiscard]] std::string display() const
    {
        std::string t = title;
        if (!subtitle.empty()) t += " — " + subtitle;
        return t;
    }
};

} // namespace myytm::models
