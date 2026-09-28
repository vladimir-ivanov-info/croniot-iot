#ifndef CRONIOT_LOG_LINEPARSER_H
#define CRONIOT_LOG_LINEPARSER_H

#include <string>

#include "Level.h"
#include "LogRecord.h"

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
//
// Host-side/off-hot-path use only. On the device, LogRouter::vprintfHook()
// runs on whatever task called ESP_LOGx - including ESP-IDF's own small
// internal tasks (WiFi driver, lwIP...) - so it uses parseEspLogLineFast()
// below instead: this std::string-returning version does several heap
// allocations per call (the `std::string line = rawLine` copy plus each
// substr()), which is exactly the "no malloc in the hook" rule the plan
// requires, and was the root cause of a real on-device stack/heap
// corruption crash during WiFi init (garbled log lines immediately
// followed by a Guru Meditation Load access fault in the WiFi task).
ParsedLine parseEspLogLine(const std::string& rawLine);

// Zero-allocation counterpart to parseEspLogLine(), for the on-device hot
// path. Parses directly from the (buf, len) span - no std::string, no
// heap allocation, no dynamic-size locals - and writes level/uptime/tag/
// message straight into `out` via LogRecord's length-bounded setters
// (truncating exactly like LogRecord's own tag/message limits). `buf`
// need not be NUL-terminated at `len`. Returns false for the same "not
// our v1 shape" cases parseEspLogLine() returns invalid for, leaving
// `out` untouched.
bool parseEspLogLineFast(const char* buf, size_t len, LogRecord& out);

}  // namespace croniot::log

#endif
