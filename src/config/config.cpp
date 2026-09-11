#include "config/config.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

namespace myytm::config {

namespace {

std::string trim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}

std::string escapeJson(const std::string& s)
{
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

std::string unescapeJson(const std::string& s)
{
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == '"') o += '"';
            else if (n == '\\') o += '\\';
            else if (n == 'n') o += '\n';
            else o += n;
        } else o += s[i];
    }
    return o;
}

int parseIntField(const std::string& json, const std::string& key, int def)
{
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    bool neg = false;
    if (pos < json.size() && json[pos] == '-') { neg = true; ++pos; }
    int v = 0;
    bool any = false;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        v = v * 10 + (json[pos] - '0');
        ++pos; any = true;
    }
    if (!any) return def;
    return neg ? -v : v;
}

bool parseBoolField(const std::string& json, const std::string& key, bool def)
{
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return def;
}

std::string parseStringField(const std::string& json, const std::string& key, const std::string& def)
{
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '"') return def;
    ++pos;
    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < json.size()) {
            char e = json[pos++];
            if (e == '"') out += '"';
            else if (e == '\\') out += '\\';
            else if (e == 'n') out += '\n';
            else out += e;
        } else out += c;
    }
    return out;
}

} // anonymous

ConfigManager::ConfigManager(std::filesystem::path filePath)
{
    if (filePath.empty()) filePath_ = defaultPath();
    else filePath_ = std::move(filePath);
}

std::filesystem::path ConfigManager::defaultPath() const
{
#ifdef _WIN32
    PWSTR path = nullptr;
    std::filesystem::path base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        base = path;
        CoTaskMemFree(path);
    } else {
        base = std::filesystem::temp_directory_path();
    }
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : std::filesystem::temp_directory_path();
#endif
    return base / "MyYtm" / "config.json";
}

void ConfigManager::setVolume(int v) { cfg_.volume = std::clamp(v, 0, 100); }
void ConfigManager::setLastScreen(std::string s) { cfg_.lastScreen = std::move(s); }

bool ConfigManager::load()
{
    lastError_.clear();
    std::ifstream in(filePath_);
    if (!in) return true; // no config yet — defaults
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (json.empty()) return true;
    // Never assume valid — handle missing/malformed per Networking guidance
    try {
        cfg_.volume = std::clamp(parseIntField(json, "volume", cfg_.volume), 0, 100);
        cfg_.showHelpHints = parseBoolField(json, "showHelpHints", cfg_.showHelpHints);
        cfg_.theme = parseStringField(json, "theme", cfg_.theme);
        cfg_.cacheSizeMb = std::clamp(parseIntField(json, "cacheSizeMb", cfg_.cacheSizeMb), 0, 10000);
        cfg_.lastScreen = parseStringField(json, "lastScreen", cfg_.lastScreen);
        if (!cfg_.isValid()) {
            cfg_ = Config{};
            lastError_ = "Invalid config values; restored defaults.";
            return false;
        }
    } catch (...) {
        lastError_ = "Failed to parse config; using defaults.";
        return false;
    }
    return true;
}

bool ConfigManager::save() const
{
    try {
        std::filesystem::create_directories(filePath_.parent_path());
        std::ofstream out(filePath_, std::ios::trunc);
        if (!out) return false;
        out << "{\n";
        out << "  \"volume\": " << cfg_.volume << ",\n";
        out << "  \"showHelpHints\": " << (cfg_.showHelpHints ? "true" : "false") << ",\n";
        out << "  \"theme\": \"" << escapeJson(cfg_.theme) << "\",\n";
        out << "  \"cacheSizeMb\": " << cfg_.cacheSizeMb << ",\n";
        out << "  \"lastScreen\": \"" << escapeJson(cfg_.lastScreen) << "\"\n";
        out << "}\n";
        return out.good();
    } catch (...) {
        return false;
    }
}

} // namespace myytm::config
