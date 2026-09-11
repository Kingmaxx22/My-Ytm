#pragma once

#include "ui/screen.h"

#include <vector>
#include <string>

namespace myytm::ui {

class HelpScreen final : public Screen {
public:
    HelpScreen();

    std::string_view name() const override { return "Help"; }
    std::string_view title() const override { return "My-Ytm — Help"; }

    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

    [[nodiscard]] size_t selected() const noexcept { return selected_; }
    void moveDown();
    void moveUp();
    void goTop();
    void goBottom();

private:
    std::vector<std::string> lines_;
    size_t selected_ = 0;
    bool pendingG_ = false;
};

} // namespace myytm::ui
