#pragma once

#include <memory>
#include <vector>

namespace myytm::app {

class App {
public:
    App() = default;
    ~App() = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();
};

} // namespace myytm::app
