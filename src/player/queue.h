#pragma once

#include "models/queue_item.h"

#include <optional>
#include <vector>

namespace myytm::player {

class Queue {
public:
    Queue() = default;

    void addNext(const models::QueueItem& item);
    void addToEnd(const models::QueueItem& item);
    bool removeAt(size_t idx);
    void clear() noexcept;
    void removeCurrent();

    [[nodiscard]] size_t size() const noexcept { return items_.size(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }

    [[nodiscard]] std::optional<size_t> currentIndex() const noexcept { return currentIdx_; }
    [[nodiscard]] const models::QueueItem* current() const noexcept;
    [[nodiscard]] const models::QueueItem* at(size_t idx) const noexcept;
    [[nodiscard]] const std::vector<models::QueueItem>& items() const noexcept { return items_; }

    // Navigation — updates currentIdx_
    bool next();
    bool previous();
    void setCurrent(size_t idx);

    // Move item (for future drag/reorder)
    bool move(size_t from, size_t to);

private:
    std::vector<models::QueueItem> items_;
    std::optional<size_t> currentIdx_;
};

} // namespace myytm::player
