#pragma once

#include "auth/auth_manager.h"
#include "ui/screen.h"

#include <memory>
#include <string>

namespace myytm::ui {

class AuthScreen final : public Screen {
public:
    explicit AuthScreen(std::shared_ptr<auth::AuthManager> mgr) : auth_(std::move(mgr)) {}

    std::string_view name() const override { return "Account"; }
    std::string_view title() const override { return "My-Ytm — Account"; }

    void render(const Renderer& r) override;
    bool handleKey(const Key& key) override;

private:
    std::shared_ptr<auth::AuthManager> auth_;
    std::string msg_;
    size_t selected_ = 0;
};

} // namespace myytm::ui
