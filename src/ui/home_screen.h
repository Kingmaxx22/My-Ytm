#pragma once

#include "ui/screen.h"

#include <string>
#include <vector>

namespace myytm::ui {

class HomeScreen final : public Screen {
public:
    HomeScreen();

    std::string_view name() const override { return "Home"; }
    std::string_view title() const override { return "My-Ytm — Home"; }

    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

    // Exposed for tests
    [[nodiscard]] size_t selected() const noexcept { return selected_; }
    [[nodiscard]] size_t itemCount() const noexcept { return items_.size(); }
    void moveDown();
    void moveUp();
    void goTop();
    void goBottom();
    void halfPageDown();
    void halfPageUp();

private:
    std::vector<std::string> items_;
    size_t selected_ = 0;
    bool pendingG_ = false;
};

} // namespace myytm::ui
