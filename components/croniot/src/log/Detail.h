#ifndef CRONIOT_LOG_DETAIL_H
#define CRONIOT_LOG_DETAIL_H

#include <cstdint>

namespace croniot::log {

// Verbosity: how much a sink renders of each captured record. Independent
// of Level (which decides *whether* a record is captured at all) - a sink
// can be Trace+Compact (every record, short line) or Error+Full (only
// errors, but with everything attached).
enum class Detail : uint8_t {
    Compact = 0,  // level, tag, message
    Normal = 1,   // + MDC context, span durations, error codes
    Full = 2,     // + trimmed payloads, hex dumps, arguments, stack
};

inline const char* toString(Detail detail) {
    switch (detail) {
        case Detail::Compact: return "COMPACT";
        case Detail::Normal: return "NORMAL";
        case Detail::Full: return "FULL";
    }
    return "UNKNOWN";
}

}  // namespace croniot::log

#endif
