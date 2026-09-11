#pragma once
#include <memory>
#include <string>

namespace myytm::platform {

class IBrowserLauncher {
public:
    virtual ~IBrowserLauncher() = default;
    virtual bool launch(const std::string& url) = 0;
};

std::unique_ptr<IBrowserLauncher> makeBrowserLauncher();

} // namespace myytm::platform
