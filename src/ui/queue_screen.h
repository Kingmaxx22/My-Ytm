#pragma once

#include "player/mock_player.h"
#include "player/queue.h"
#include "ui/screen.h"

#include <memory>

namespace myytm::ui {

class QueueScreen final : public Screen {
public:
    QueueScreen(std::shared_ptr<player::Queue> queue, std::shared_ptr<player::Player> player)
        : queue_(std::move(queue)), player_(std::move(player)) {}

    std::string_view name() const override { return "Queue"; }
    std::string_view title() const override { return "My-Ytm — Queue"; }

    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

private:
    void moveDown();
    void moveUp();
    void goTop();
    void goBottom();

    std::shared_ptr<player::Queue> queue_;
    std::shared_ptr<player::Player> player_;
    size_t selected_ = 0;
    bool pendingG_ = false;
    std::string msg_;
};

} // namespace myytm::ui
