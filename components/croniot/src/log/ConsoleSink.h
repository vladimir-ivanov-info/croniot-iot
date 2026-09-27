#ifndef CRONIOT_LOG_CONSOLESINK_H
#define CRONIOT_LOG_CONSOLESINK_H

#include "esp_log.h"

#include "LogRecord.h"

namespace croniot::log {

enum class ConsoleFormat {
    Text,   // mirrors today's ESP-IDF line shape: "<L> (<uptime_ms>) <tag>: <msg>"
    Jsonl,  // one compact cJSON object per line: {"ts":..,"up":..,"lvl":..,"tag":..,"msg":..}
};

// Renders a LogRecord and writes it out through `rawWrite` - the raw
// vprintf-like function LogRouter saved before installing its own hook
// (see LogRouter::previousHook()) - rather than reimplementing UART
// output. This is deliberately the *only* place that ever produces
// console bytes: same code path whether the record is brand new (drained
// moments after the hook pushed it) or leftover from a previous boot that
// survived in NoinitRing across a crash.
class ConsoleSink {
public:
    static void write(const LogRecord& record, ConsoleFormat format, vprintf_like_t rawWrite);

private:
    // "ts" is left equal to "up" (uptimeMs) for now - there's no epoch
    // clock until SNTP exists (a later phase); document that rather than
    // silently pretend "ts" means wall-clock time.
    static void writeJsonl(const LogRecord& record, vprintf_like_t rawWrite);
    static void writeText(const LogRecord& record, vprintf_like_t rawWrite);
};

}  // namespace croniot::log

#endif
