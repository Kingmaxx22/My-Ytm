#include "app/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

namespace myytm::app {

Logger::Logger(std::filesystem::path filePath)
{
    if (filePath.empty()) filePath_ = defaultPath();
    else filePath_ = std::move(filePath);

    try {
        std::filesystem::create_directories(filePath_.parent_path());
        out_.open(filePath_, std::ios::app);
    } catch (...) {}
}

Logger::~Logger()
{
    if (out_.is_open()) out_.close();
}

std::filesystem::path Logger::defaultPath() const
{
#ifdef _WIN32
    PWSTR path = nullptr;
    std::filesystem::path base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        base = path;
        CoTaskMemFree(path);
    } else base = std::filesystem::temp_directory_path();
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : std::filesystem::temp_directory_path();
#endif
    return base / "MyYtm" / "myytm.log";
}

std::string Logger::levelStr(LogLevel lvl) noexcept
{
    switch (lvl) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::log(LogLevel lvl, std::string_view msg)
{
    if (!out_.is_open()) return;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    out_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << levelStr(lvl) << "] " << msg << "\n";
    out_.flush();
}

} // namespace myytm::app
