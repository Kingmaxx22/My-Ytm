#pragma once

#include "models/search_result.h"
#include "ui/screen.h"

#include <memory>
#include <optional>
#include <vector>

namespace myytm::youtube { class YouTubeClient; }
namespace myytm::player { class Queue; class Player; }

namespace myytm::ui {

class PlaylistsScreen final : public Screen {
public:
    PlaylistsScreen(std::shared_ptr<youtube::YouTubeClient> client,
                    std::shared_ptr<player::Queue> queue,
                    std::shared_ptr<player::Player> player)
        : client_(std::move(client)), queue_(std::move(queue)), player_(std::move(player)) {}

    std::string_view name() const override { return "Playlists"; }
    std::string_view title() const override { return "My-Ytm — Playlists"; }

    void onEnter() override;
    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;
    [[nodiscard]] std::optional<std::string> continuationToken() const noexcept { return continuationToken_; }
    bool loadNextPage();

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
    std::optional<std::string> continuationToken_;
};

} // namespace myytm::ui
