#pragma once

#include "models/search_result.h"
#include "ui/screen.h"

#include <memory>
#include <string>
#include <vector>

namespace myytm::youtube { class YouTubeClient; }
namespace myytm::player { class Queue; class Player; }

namespace myytm::ui {

class SearchScreen final : public Screen {
public:
    SearchScreen();
    explicit SearchScreen(std::shared_ptr<youtube::YouTubeClient> client);

    std::string_view name() const override { return "Search"; }
    std::string_view title() const override { return "My-Ytm — Search"; }

    void onEnter() override {}
    void onExit() override {}

    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

    // Test hooks
    [[nodiscard]] bool isInputMode() const noexcept { return mode_ == Mode::Input; }
    [[nodiscard]] std::string_view query() const noexcept { return query_; }
    [[nodiscard]] size_t selected() const noexcept { return selected_; }
    [[nodiscard]] size_t resultCount() const noexcept { return results_.size(); }
    [[nodiscard]] const models::SearchResults& results() const noexcept { return results_; }
    void setQuery(std::string q);
    void executeSearch();
    void setClient(std::shared_ptr<youtube::YouTubeClient> c) { client_ = std::move(c); }
    void setQueue(std::shared_ptr<player::Queue> q) { queue_ = std::move(q); }
    void setPlayer(std::shared_ptr<player::Player> p) { player_ = std::move(p); }

private:
    enum class Mode { Browsing, Input };

    void moveDown();
    void moveUp();
    void goTop();
    void goBottom();
    void halfPageDown();
    void halfPageUp();

    static std::string toLower(std::string_view s);
    static bool containsCi(std::string_view haystack, std::string_view needle);

    Mode mode_ = Mode::Browsing;
    std::string query_;
    std::string committedQuery_;
    models::SearchResults catalog_;
    models::SearchResults results_;
    size_t selected_ = 0;
    bool pendingG_ = false;
    std::string statusMsg_;
    std::shared_ptr<youtube::YouTubeClient> client_;
    std::shared_ptr<player::Queue> queue_;
    std::shared_ptr<player::Player> player_;
};

} // namespace myytm::ui
