#include "platform/browser/IBrowserLauncher.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace myytm::platform {

class BrowserLauncherWin final : public IBrowserLauncher {
public:
    bool launch(const std::string& url) override {
#ifdef _WIN32
        HINSTANCE r = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<intptr_t>(r) > 32;
#else
        (void)url; return false;
#endif
    }
};

std::unique_ptr<IBrowserLauncher> makeBrowserLauncher() {
    return std::make_unique<BrowserLauncherWin>();
}

} // namespace myytm::platform
