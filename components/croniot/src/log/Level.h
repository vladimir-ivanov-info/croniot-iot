#ifndef CRONIOT_LOG_LEVEL_H
#define CRONIOT_LOG_LEVEL_H

#include <cstdint>

namespace croniot::log {

// Severity: what importance the event has. Lower value = more severe,
// mirroring ESP-IDF's own esp_log_level_t ordering so the two map directly.
enum class Level : uint8_t {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,  // ESP-IDF calls this Verbose; Croniot calls it Trace.
};

inline const char* toString(Level level) {
    switch (level) {
        case Level::Error: return "ERROR";
        case Level::Warn: return "WARN";
        case Level::Info: return "INFO";
        case Level::Debug: return "DEBUG";
        case Level::Trace: return "TRACE";
    }
    return "UNKNOWN";
}

// True if a record at `recordLevel` should be captured when the configured
// threshold is `configuredLevel` (e.g. Error record, Info threshold -> true;
// Debug record, Info threshold -> false).
inline bool meetsThreshold(Level recordLevel, Level configuredLevel) {
    return static_cast<uint8_t>(recordLevel) <= static_cast<uint8_t>(configuredLevel);
}

}  // namespace croniot::log

#endif
