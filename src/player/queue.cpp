#include "player/queue.h"

namespace myytm::player {

void Queue::addNext(const models::QueueItem& item)
{
    if (items_.empty() || !currentIdx_.has_value()) {
        items_.push_back(item);
        if (!currentIdx_.has_value()) currentIdx_ = 0;
        return;
    }
    size_t pos = *currentIdx_ + 1;
    if (pos > items_.size()) pos = items_.size();
    items_.insert(items_.begin() + static_cast<long long>(pos), item);
}

void Queue::addToEnd(const models::QueueItem& item)
{
    items_.push_back(item);
    if (!currentIdx_.has_value()) currentIdx_ = 0;
}

bool Queue::removeAt(size_t idx)
{
    if (idx >= items_.size()) return false;
    items_.erase(items_.begin() + static_cast<long long>(idx));
    if (items_.empty()) {
        currentIdx_.reset();
        return true;
    }
    if (!currentIdx_.has_value()) return true;
    size_t cur = *currentIdx_;
    if (idx < cur) currentIdx_ = cur - 1;
    else if (idx == cur) {
        if (cur >= items_.size()) currentIdx_ = items_.size() - 1;
        // else stays at same index (next item slides in)
    }
    return true;
}

void Queue::clear() noexcept
{
    items_.clear();
    currentIdx_.reset();
}

void Queue::removeCurrent()
{
    if (currentIdx_) removeAt(*currentIdx_);
}

const models::QueueItem* Queue::current() const noexcept
{
    if (!currentIdx_ || *currentIdx_ >= items_.size()) return nullptr;
    return &items_[*currentIdx_];
}

const models::QueueItem* Queue::at(size_t idx) const noexcept
{
    if (idx >= items_.size()) return nullptr;
    return &items_[idx];
}

bool Queue::next()
{
    if (!currentIdx_ || items_.empty()) return false;
    if (*currentIdx_ + 1 < items_.size()) {
        ++(*currentIdx_);
        return true;
    }
    return false;
}

bool Queue::previous()
{
    if (!currentIdx_ || *currentIdx_ == 0) return false;
    --(*currentIdx_);
    return true;
}

void Queue::setCurrent(size_t idx)
{
    if (idx < items_.size()) currentIdx_ = idx;
}

bool Queue::move(size_t from, size_t to)
{
    if (from >= items_.size() || to >= items_.size() || from == to) return false;
    auto item = items_[from];
    items_.erase(items_.begin() + static_cast<long long>(from));
    items_.insert(items_.begin() + static_cast<long long>(to), item);

    // Adjust currentIdx_
    if (!currentIdx_) return true;
    size_t cur = *currentIdx_;
    if (cur == from) currentIdx_ = to;
    else if (from < cur && to >= cur) --(*currentIdx_);
    else if (from > cur && to <= cur) ++(*currentIdx_);
    return true;
}

} // namespace myytm::player
