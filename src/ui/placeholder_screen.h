#pragma once

#include "ui/screen.h"

#include <string>

namespace myytm::ui {

class PlaceholderScreen final : public Screen {
public:
    PlaceholderScreen(std::string name, std::string title, std::string body)
        : name_(std::move(name)), title_(std::move(title)), body_(std::move(body)) {}

    std::string_view name() const override { return name_; }
    std::string_view title() const override { return title_; }

    void render(const Renderer& r) override;
    bool handleKey(const Key&) override { return false; }

private:
    std::string name_;
    std::string title_;
    std::string body_;
};

} // namespace myytm::ui
