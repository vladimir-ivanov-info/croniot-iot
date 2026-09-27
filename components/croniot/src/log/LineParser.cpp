#include "LineParser.h"

#include <cctype>

namespace croniot::log {

namespace {

Level levelFromChar(char c) {
    switch (c) {
        case 'E': return Level::Error;
        case 'W': return Level::Warn;
        case 'I': return Level::Info;
        case 'D': return Level::Debug;
        case 'V': return Level::Trace;  // ESP-IDF's Verbose
        default: return Level::Info;
    }
}

bool isLevelChar(char c) {
    return c == 'E' || c == 'W' || c == 'I' || c == 'D' || c == 'V';
}

// Strips one leading ANSI CSI sequence ("\033[" ... "m") if present and
// returns how many bytes it occupied (0 if there isn't one).
size_t leadingAnsiLength(const std::string& s) {
    if (s.size() < 3 || s[0] != '\033' || s[1] != '[') return 0;
    size_t i = 2;
    while (i < s.size() && s[i] != 'm') {
        ++i;
    }
    if (i >= s.size()) return 0;  // no closing 'm': not a real CSI sequence
    return i + 1;
}

}  // namespace

ParsedLine parseEspLogLine(const std::string& rawLine) {
    ParsedLine result;

    std::string line = rawLine;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    static const std::string kAnsiReset = "\033[0m";
    if (line.size() >= kAnsiReset.size() &&
        line.compare(line.size() - kAnsiReset.size(), kAnsiReset.size(), kAnsiReset) == 0) {
        line.resize(line.size() - kAnsiReset.size());
    }

    line.erase(0, leadingAnsiLength(line));

    if (line.size() < 2 || !isLevelChar(line[0]) || line[1] != ' ') {
        return result;
    }
    Level level = levelFromChar(line[0]);

    size_t pos = 2;
    if (pos >= line.size() || line[pos] != '(') return result;
    ++pos;

    size_t digitsStart = pos;
    while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    if (pos == digitsStart) return result;  // no digits between '(' and ')'

    uint64_t uptimeMs = 0;
    for (size_t i = digitsStart; i < pos; ++i) {
        uptimeMs = uptimeMs * 10 + static_cast<uint64_t>(line[i] - '0');
    }

    if (pos >= line.size() || line[pos] != ')') return result;
    ++pos;
    if (pos >= line.size() || line[pos] != ' ') return result;
    ++pos;

    size_t tagStart = pos;
    size_t colonPos = line.find(": ", tagStart);
    if (colonPos == std::string::npos) return result;

    std::string tag = line.substr(tagStart, colonPos - tagStart);
    if (tag.empty()) return result;

    result.valid = true;
    result.level = level;
    result.uptimeMs = uptimeMs;
    result.tag = std::move(tag);
    result.message = line.substr(colonPos + 2);
    return result;
}

}  // namespace croniot::log
