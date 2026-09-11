#pragma once

#include "ui/key.h"

#include <string_view>

namespace myytm::ui {

class Renderer;

class Screen {
public:
    virtual ~Screen() = default;

    virtual std::string_view name() const = 0;
    virtual std::string_view title() const = 0;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual void render(const Renderer& r) = 0;

    // Return true if key was consumed.
    virtual bool handleKey(const Key& key) = 0;
};

} // namespace myytm::ui
