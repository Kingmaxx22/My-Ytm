#pragma once

#include "models/search_result.h"
#include "ui/screen.h"

#include <memory>
#include <string>
#include <vector>

namespace myytm::youtube { class YouTubeClient; }
namespace myytm::player { class Queue; class Player; }

namespace myytm::ui {

class LibraryScreen final : public Screen {
public:
    LibraryScreen(std::shared_ptr<youtube::YouTubeClient> client,
                  std::shared_ptr<player::Queue> queue,
                  std::shared_ptr<player::Player> player)
        : client_(std::move(client)), queue_(std::move(queue)), player_(std::move(player)) {}

    std::string_view name() const override { return "Library"; }
    std::string_view title() const override { return "My-Ytm — Library"; }

    void onEnter() override;
    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

private:
    void reload();
    void moveDown();
    void moveUp();
    void goTop();
    void goBottom();

    std::shared_ptr<youtube::YouTubeClient> client_;
    std::shared_ptr<player::Queue> queue_;
    std::shared_ptr<player::Player> player_;

    std::vector<models::SearchResult> items_;
    size_t selected_ = 0;
    bool pendingG_ = false;
    std::string statusMsg_;
    bool loading_ = false;
};

} // namespace myytm::ui
