#include "RemoteLogConfig.h"

#include <cctype>
#include <cstdlib>

namespace croniot::log {

namespace {

// Minimal hand-rolled scanner for exactly the JSON subset this config
// shape needs: objects, strings (with the four escapes JSON producers
// actually emit), and non-negative integers - see RemoteLogConfig.h for
// why this isn't a general JSON parser.
class JsonScanner {
public:
    explicit JsonScanner(const std::string& s) : s_(s) {}

    bool ok() const { return ok_; }

    void skipWs() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    bool consume(char c) {
        skipWs();
        if (pos_ >= s_.size() || s_[pos_] != c) {
            ok_ = false;
            return false;
        }
        ++pos_;
        return true;
    }

    char peek() {
        skipWs();
        return pos_ < s_.size() ? s_[pos_] : '\0';
    }

    std::optional<std::string> parseString() {
        skipWs();
        if (pos_ >= s_.size() || s_[pos_] != '"') {
            ok_ = false;
            return std::nullopt;
        }
        ++pos_;
        std::string out;
        while (pos_ < s_.size() && s_[pos_] != '"') {
            char c = s_[pos_++];
            if (c == '\\' && pos_ < s_.size()) {
                char esc = s_[pos_++];
                switch (esc) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    default: out += esc; break;
                }
            } else {
                out += c;
            }
        }
        if (pos_ >= s_.size()) {
            ok_ = false;
            return std::nullopt;
        }
        ++pos_;  // closing quote
        return out;
    }

    std::optional<int64_t> parseInt() {
        skipWs();
        size_t start = pos_;
        if (pos_ < s_.size() && s_[pos_] == '-') ++pos_;
        size_t digitsStart = pos_;
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        if (pos_ == digitsStart) {
            ok_ = false;
            return std::nullopt;
        }
        return std::strtoll(s_.c_str() + start, nullptr, 10);
    }

    // Skips one value of any JSON type - used for keys this parser
    // doesn't recognize, so a field a future server adds doesn't break
    // parsing of the rest of the object.
    void skipValue() {
        skipWs();
        if (pos_ >= s_.size()) {
            ok_ = false;
            return;
        }
        char c = s_[pos_];
        if (c == '"') {
            parseString();
            return;
        }
        if (c == '{' || c == '[') {
            char open = c;
            char close = (c == '{') ? '}' : ']';
            int depth = 0;
            bool inString = false;
            while (pos_ < s_.size()) {
                char ch = s_[pos_++];
                if (inString) {
                    if (ch == '\\' && pos_ < s_.size()) {
                        ++pos_;
                        continue;
                    }
                    if (ch == '"') inString = false;
                    continue;
                }
                if (ch == '"') {
                    inString = true;
                    continue;
                }
                if (ch == open) {
                    ++depth;
                } else if (ch == close) {
                    --depth;
                    if (depth == 0) return;
                }
            }
            ok_ = false;
            return;
        }
        // number / true / false / null - skip to the next delimiter.
        while (pos_ < s_.size() && s_[pos_] != ',' && s_[pos_] != '}' && s_[pos_] != ']' &&
               !std::isspace(static_cast<unsigned char>(s_[pos_]))) {
            ++pos_;
        }
    }

private:
    const std::string& s_;
    size_t pos_ = 0;
    bool ok_ = true;
};

std::optional<Level> parseLevelName(const std::string& name) {
    if (name == "error") return Level::Error;
    if (name == "warn") return Level::Warn;
    if (name == "info") return Level::Info;
    if (name == "debug") return Level::Debug;
    if (name == "trace") return Level::Trace;
    return std::nullopt;
}

}  // namespace

std::optional<RemoteLogConfig> parseRemoteLogConfig(const std::string& json) {
    JsonScanner scanner(json);
    if (!scanner.consume('{')) return std::nullopt;

    RemoteLogConfig config;

    if (scanner.peek() != '}') {
        while (true) {
            auto key = scanner.parseString();
            if (!key || !scanner.ok() || !scanner.consume(':')) return std::nullopt;

            if (*key == "default") {
                auto value = scanner.parseString();
                if (!value || !scanner.ok()) return std::nullopt;
                auto level = parseLevelName(*value);
                if (!level) return std::nullopt;
                config.defaultLevel = level;
            } else if (*key == "tags") {
                if (!scanner.consume('{')) return std::nullopt;
                if (scanner.peek() != '}') {
                    while (true) {
                        auto tag = scanner.parseString();
                        if (!tag || !scanner.ok() || !scanner.consume(':')) return std::nullopt;
                        auto value = scanner.parseString();
                        if (!value || !scanner.ok()) return std::nullopt;
                        auto level = parseLevelName(*value);
                        if (!level) return std::nullopt;
                        config.tagLevels.emplace_back(*tag, *level);
                        if (scanner.peek() == ',') {
                            scanner.consume(',');
                            continue;
                        }
                        break;
                    }
                }
                if (!scanner.consume('}')) return std::nullopt;
            } else if (*key == "ttlSec") {
                auto value = scanner.parseInt();
                if (!value || !scanner.ok() || *value < 0) return std::nullopt;
                config.ttlSec = static_cast<uint32_t>(*value);
            } else {
                scanner.skipValue();
                if (!scanner.ok()) return std::nullopt;
            }

            if (scanner.peek() == ',') {
                scanner.consume(',');
                continue;
            }
            break;
        }
    }

    if (!scanner.consume('}')) return std::nullopt;
    return config;
}

}  // namespace croniot::log
