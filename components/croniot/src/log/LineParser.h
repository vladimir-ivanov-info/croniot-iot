#ifndef CRONIOT_LOG_LINEPARSER_H
#define CRONIOT_LOG_LINEPARSER_H

#include <string>

#include "Level.h"

namespace croniot::log {

struct ParsedLine {
    bool valid = false;
    Level level = Level::Info;
    uint64_t uptimeMs = 0;
    std::string tag;
    std::string message;  // trailing newline stripped; ANSI reset stripped
};

// Parses one ESP-IDF v1-format log line: "<L> (<uptime_ms>) <tag>: <msg>",
// optionally wrapped in ANSI colour codes (CONFIG_LOG_COLORS=y prepends
// "\033[0;<n>m" and appends "\033[0m"). <L> is one of E/W/I/D/V (V maps to
// Level::Trace). Returns valid=false if the line doesn't match this shape
// at all (e.g. a raw printf from third-party code with no tag).
ParsedLine parseEspLogLine(const std::string& rawLine);

}  // namespace croniot::log

#endif
