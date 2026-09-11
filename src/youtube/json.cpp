#include "youtube/json.h"
#include <cctype>
#include <cmath>
#include <sstream>

namespace myytm::youtube::json {

namespace {

void skipWs(std::string_view s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

struct Parser {
    std::string_view s;
    size_t pos = 0;
    std::string error;
    bool fail(const std::string& msg) { error = msg + " at " + std::to_string(pos); return false; }

    bool parseValue(JsonValue& out);
    bool parseObject(JsonValue& out);
    bool parseArray(JsonValue& out);
    bool parseString(std::string& out);
    bool parseNumber(JsonValue& out);
    bool parseLiteral(JsonValue& out);
};

bool Parser::parseString(std::string& out) {
    skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '"') { fail("Expected '\"'"); return false; }
    ++pos;
    out.clear();
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') return true;
        if (c == '\\') {
            if (pos >= s.size()) { fail("Unterminated escape"); return false; }
            char e = s[pos++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (pos + 4 > s.size()) { fail("Invalid \\u"); return false; }
                    // Simplified: just parse 4 hex and convert to UTF-8 (BMP only)
                    std::string hex(s.substr(pos,4));
                    pos+=4;
                    unsigned int code=0;
                    for(char hc: hex){ code*=16; if(hc>='0'&&hc<='9') code+=hc-'0'; else if(hc>='a'&&hc<='f') code+=10+hc-'a'; else if(hc>='A'&&hc<='F') code+=10+hc-'A'; else {fail("Invalid hex"); return false;}}
                    if (code < 0x80) out.push_back(static_cast<char>(code));
                    else if (code < 0x800) { out.push_back(static_cast<char>(0xC0 | (code>>6))); out.push_back(static_cast<char>(0x80 | (code & 0x3F))); }
                    else { out.push_back(static_cast<char>(0xE0 | (code>>12))); out.push_back(static_cast<char>(0x80 | ((code>>6)&0x3F))); out.push_back(static_cast<char>(0x80 | (code &0x3F))); }
                    break;
                }
                default: out.push_back(e); break;
            }
        } else out.push_back(c);
    }
    fail("Unterminated string");
    return false;
}

bool Parser::parseNumber(JsonValue& out) {
    skipWs(s, pos);
    size_t start = pos;
    if (pos < s.size() && s[pos]=='-') ++pos;
    if (pos >= s.size() || !std::isdigit((unsigned char)s[pos])) { fail("Expected number"); return false; }
    while (pos < s.size() && std::isdigit((unsigned char)s[pos])) ++pos;
    if (pos < s.size() && s[pos]=='.') { ++pos; while (pos < s.size() && std::isdigit((unsigned char)s[pos])) ++pos; }
    if (pos < s.size() && (s[pos]=='e' || s[pos]=='E')) {
        ++pos; if (pos < s.size() && (s[pos]=='+'||s[pos]=='-')) ++pos;
        while (pos < s.size() && std::isdigit((unsigned char)s[pos])) ++pos;
    }
    std::string numStr(s.substr(start, pos-start));
    try { double d = std::stod(numStr); out.type=JsonValue::Type::Number; out.value=d; return true; } catch(...) { fail("Invalid number"); return false; }
}

bool Parser::parseLiteral(JsonValue& out) {
    skipWs(s, pos);
    if (s.substr(pos,4)=="true") { pos+=4; out.type=JsonValue::Type::Bool; out.value=true; return true; }
    if (s.substr(pos,5)=="false") { pos+=5; out.type=JsonValue::Type::Bool; out.value=false; return true; }
    if (s.substr(pos,4)=="null") { pos+=4; out.type=JsonValue::Type::Null; out.value=nullptr; return true; }
    fail("Expected literal");
    return false;
}

bool Parser::parseArray(JsonValue& out) {
    skipWs(s, pos);
    if (pos >= s.size() || s[pos]!='[') { fail("Expected '['"); return false; }
    ++pos;
    std::vector<JsonValue> arr;
    skipWs(s, pos);
    if (pos < s.size() && s[pos]==']') { ++pos; out.type=JsonValue::Type::Array; out.value=std::move(arr); return true; }
    while (true) {
        JsonValue elem;
        if (!parseValue(elem)) return false;
        arr.push_back(std::move(elem));
        skipWs(s, pos);
        if (pos >= s.size()) { fail("Unterminated array"); return false; }
        if (s[pos]==']') { ++pos; break; }
        if (s[pos]==',') { ++pos; continue; }
        fail("Expected ',' or ']'");
        return false;
    }
    out.type=JsonValue::Type::Array; out.value=std::move(arr); return true;
}

bool Parser::parseObject(JsonValue& out) {
    skipWs(s, pos);
    if (pos >= s.size() || s[pos]!='{') { fail("Expected '{'"); return false; }
    ++pos;
    std::map<std::string, JsonValue> obj;
    skipWs(s, pos);
    if (pos < s.size() && s[pos]=='}') { ++pos; out.type=JsonValue::Type::Object; out.value=std::move(obj); return true; }
    while (true) {
        std::string key;
        if (!parseString(key)) return false;
        skipWs(s, pos);
        if (pos>=s.size()||s[pos]!=':') { fail("Expected ':'"); return false; }
        ++pos;
        JsonValue val;
        if (!parseValue(val)) return false;
        obj.emplace(std::move(key), std::move(val));
        skipWs(s, pos);
        if (pos>=s.size()) { fail("Unterminated object"); return false; }
        if (s[pos]=='}') { ++pos; break; }
        if (s[pos]==',') { ++pos; continue; }
        fail("Expected ',' or '}'"); return false;
    }
    out.type=JsonValue::Type::Object; out.value=std::move(obj); return true;
}

bool Parser::parseValue(JsonValue& out) {
    skipWs(s, pos);
    if (pos>=s.size()) { fail("Unexpected end"); return false; }
    char c = s[pos];
    if (c=='"') { std::string str; if(!parseString(str)) return false; out.type=JsonValue::Type::String; out.value=std::move(str); return true; }
    if (c=='{') return parseObject(out);
    if (c=='[') return parseArray(out);
    if (c=='-' || std::isdigit((unsigned char)c)) return parseNumber(out);
    if (c=='t' || c=='f' || c=='n') return parseLiteral(out);
    fail(std::string("Unexpected char '")+c+"'");
    return false;
}

} // anonymous

ParseResult parse(std::string_view json) {
    // Strip XSSI prefix )]}'
    if (json.size() >= 4 && json.substr(0,4)==")]}'") {
        size_t nl = json.find('\n');
        if (nl != std::string_view::npos) json = json.substr(nl+1);
        else json = json.substr(4);
    }
    Parser p; p.s = json; p.pos=0;
    JsonValue root;
    if (!p.parseValue(root)) return ParseResult{false, {}, p.error, p.pos};
    skipWs(json, p.pos);
    if (p.pos != json.size()) {
        // Allow trailing ws, but not extra content
        skipWs(json, p.pos);
        if (p.pos != json.size()) return ParseResult{false, {}, "Trailing content at "+std::to_string(p.pos), p.pos};
    }
    return ParseResult{true, std::move(root), "", 0};
}

std::optional<std::string> getString(const JsonValue& obj, const std::string& key) {
    const JsonValue* v = obj.get(key);
    if (!v || !v->isString()) return std::nullopt;
    return v->asString();
}

const JsonValue* findRecursive(const JsonValue& root, const std::string& key) {
    if (root.isObject()) {
        auto& o = root.asObject();
        auto it = o.find(key);
        if (it != o.end()) return &it->second;
        for (auto& kv : o) {
            const JsonValue* f = findRecursive(kv.second, key);
            if (f) return f;
        }
    } else if (root.isArray()) {
        for (auto& e : root.asArray()) {
            const JsonValue* f = findRecursive(e, key);
            if (f) return f;
        }
    }
    return nullptr;
}

void collectRenderers(const JsonValue& root, const std::string& rendererKey, std::vector<const JsonValue*>& out) {
    if (root.isObject()) {
        auto& o = root.asObject();
        auto it = o.find(rendererKey);
        if (it != o.end()) out.push_back(&it->second);
        for (auto& kv : o) collectRenderers(kv.second, rendererKey, out);
    } else if (root.isArray()) {
        for (auto& e : root.asArray()) collectRenderers(e, rendererKey, out);
    }
}

std::string extractFirstRunText(const JsonValue* textNode) {
    if (!textNode || !textNode->isObject()) return "";
    const JsonValue* runs = textNode->get("runs");
    if (runs && runs->isArray() && !runs->asArray().empty()) {
        const JsonValue* first = runs->at(0);
        if (first) {
            auto t = getString(*first, "text");
            if (t) return *t;
        }
    }
    auto simple = getString(*textNode, "simpleText");
    if (simple) return *simple;
    return "";
}

std::string extractRunsText(const JsonValue* textNode) {
    if (!textNode || !textNode->isObject()) return "";
    const JsonValue* runs = textNode->get("runs");
    if (runs && runs->isArray()) {
        std::string out;
        for (auto& r : runs->asArray()) {
            auto t = getString(r, "text");
            if (t) out += *t;
        }
        if (!out.empty()) return out;
    }
    auto simple = getString(*textNode, "simpleText");
    if (simple) return *simple;
    return "";
}

std::string joinRuns(const JsonValue* runsNode, const std::string& sep) {
    if (!runsNode || !runsNode->isArray()) return "";
    std::string out;
    for (auto& r : runsNode->asArray()) {
        auto t = getString(r, "text");
        if (!t) continue;
        if (!out.empty()) out += sep;
        out += *t;
    }
    return out;
}

std::optional<std::string> extractThumbnail(const JsonValue* thumbnailNode) {
    if (!thumbnailNode) return std::nullopt;
    // Try musicThumbnailRenderer.thumbnail.thumbnails[0].url
    const JsonValue* t = thumbnailNode->get("musicThumbnailRenderer");
    if (t) thumbnailNode = t;
    const JsonValue* thumb = thumbnailNode->get("thumbnail");
    if (!thumb) thumb = thumbnailNode;
    const JsonValue* thumbs = thumb->get("thumbnails");
    if (!thumbs || !thumbs->isArray() || thumbs->asArray().empty()) return std::nullopt;
    const JsonValue* first = thumbs->at(0);
    if (!first) return std::nullopt;
    return getString(*first, "url");
}

std::optional<int> parseDuration(const std::string& s) {
    if (s.empty()) return std::nullopt;
    // Format "3:45" or "1:02:03" or "45"
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == ':') { parts.push_back(cur); cur.clear(); }
        else if (std::isdigit((unsigned char)c)) cur.push_back(c);
        else if (c == ' ' || c == '.' ) { /* ignore */ }
        else return std::nullopt;
    }
    parts.push_back(cur);
    if (parts.empty()) return std::nullopt;
    try {
        if (parts.size() == 1) return std::stoi(parts[0]);
        if (parts.size() == 2) return std::stoi(parts[0])*60 + std::stoi(parts[1]);
        if (parts.size() == 3) return std::stoi(parts[0])*3600 + std::stoi(parts[1])*60 + std::stoi(parts[2]);
    } catch(...) { return std::nullopt; }
    return std::nullopt;
}

} // namespace myytm::youtube::json
