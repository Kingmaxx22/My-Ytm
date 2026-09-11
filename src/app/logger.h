#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace myytm::app {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    explicit Logger(std::filesystem::path filePath = {});
    ~Logger();

    void log(LogLevel lvl, std::string_view msg);
    void info(std::string_view msg) { log(LogLevel::Info, msg); }
    void warn(std::string_view msg) { log(LogLevel::Warn, msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }
    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }

    // Never call with secrets — caller must scrub tokens/cookies/headers
    [[nodiscard]] std::filesystem::path filePath() const noexcept { return filePath_; }

private:
    std::filesystem::path filePath_;
    std::ofstream out_;
    std::filesystem::path defaultPath() const;
    static std::string levelStr(LogLevel lvl) noexcept;
};

} // namespace myytm::app
