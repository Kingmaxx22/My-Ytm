#pragma once
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace myytm::youtube::json {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    std::variant<std::nullptr_t, bool, double, std::string, std::vector<JsonValue>, std::map<std::string, JsonValue>> value;

    bool isNull() const noexcept { return type == Type::Null; }
    bool isBool() const noexcept { return type == Type::Bool; }
    bool isNumber() const noexcept { return type == Type::Number; }
    bool isString() const noexcept { return type == Type::String; }
    bool isArray() const noexcept { return type == Type::Array; }
    bool isObject() const noexcept { return type == Type::Object; }

    const std::string& asString() const { return std::get<std::string>(value); }
    double asNumber() const { return std::get<double>(value); }
    bool asBool() const { return std::get<bool>(value); }
    const std::vector<JsonValue>& asArray() const { return std::get<std::vector<JsonValue>>(value); }
    const std::map<std::string, JsonValue>& asObject() const { return std::get<std::map<std::string, JsonValue>>(value); }

    const JsonValue* get(const std::string& key) const {
        if (!isObject()) return nullptr;
        auto& o = asObject();
        auto it = o.find(key);
        if (it == o.end()) return nullptr;
        return &it->second;
    }
    const JsonValue* at(size_t idx) const {
        if (!isArray()) return nullptr;
        auto& a = asArray();
        if (idx >= a.size()) return nullptr;
        return &a[idx];
    }
};

// Strip )]}' prefix and parse. Returns nullopt on failure with error message.
struct ParseResult {
    bool ok = false;
    JsonValue value;
    std::string error;
    size_t errorPos = 0;
};

ParseResult parse(std::string_view json);

// Helpers for InnerTube parsing — never assume valid, return nullopt on missing.
std::optional<std::string> getString(const JsonValue& obj, const std::string& key);
std::optional<std::string> getStringAt(const JsonValue& arr, size_t idx, const std::string& key);
const JsonValue* findRecursive(const JsonValue& root, const std::string& key); // depth-first find first occurrence
void collectRenderers(const JsonValue& root, const std::string& rendererKey, std::vector<const JsonValue*>& out);

// Text helpers for runs
std::string joinRuns(const JsonValue* runsNode, const std::string& sep = " • ");
std::string extractFirstRunText(const JsonValue* textNode); // text.runs[0].text or text.simpleText
std::string extractRunsText(const JsonValue* textNode); // joins all runs

// Thumbnail helper
std::optional<std::string> extractThumbnail(const JsonValue* thumbnailNode);

// Duration helper: "3:45" -> 225, "1:02:03" -> 3723, missing -> nullopt
std::optional<int> parseDuration(const std::string& s);

} // namespace myytm::youtube::json
