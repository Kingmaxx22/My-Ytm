#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace myytm::config {

struct Config {
    int volume = 50;                 // 0-100
    bool showHelpHints = true;
    std::string theme = "default";   // default/dark
    int cacheSizeMb = 100;
    std::string lastScreen = "Home"; // Home/Search/...

    [[nodiscard]] bool isValid() const noexcept { return volume >= 0 && volume <= 100; }
};

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path filePath = {});

    [[nodiscard]] const Config& get() const noexcept { return cfg_; }
    Config& get() noexcept { return cfg_; }

    void setVolume(int v);
    void setLastScreen(std::string s);

    // Persists to %APPDATA%/MyYtm/config.json (or custom path for tests)
    bool load();
    bool save() const;

    [[nodiscard]] std::filesystem::path filePath() const noexcept { return filePath_; }
    [[nodiscard]] std::string_view lastError() const noexcept { return lastError_; }

private:
    std::string lastError_;
    std::filesystem::path filePath_;
    Config cfg_;

    std::filesystem::path defaultPath() const;
};

} // namespace myytm::config
