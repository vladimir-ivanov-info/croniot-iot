#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace croniot {

// Minimal hand-rolled scanner for the JSON subset the device's small
// server-to-device messages need: objects, strings (with the four
// escapes JSON producers actually emit), and integers. Not a general
// JSON parser on purpose: it compiles in test_host without cJSON, and
// both of its users (log/RemoteLogConfig.cpp, ota/OtaManifest.cpp)
// reject anything it does not understand instead of half-applying it.
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

}  // namespace croniot
