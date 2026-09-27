#include "ConsoleSink.h"

#include <cstdarg>
#include <cstdio>

#include "cJSON.h"

#include "CJsonPtr.h"

namespace croniot::log {

namespace {

char levelChar(Level level) {
    switch (level) {
        case Level::Error: return 'E';
        case Level::Warn: return 'W';
        case Level::Info: return 'I';
        case Level::Debug: return 'D';
        case Level::Trace: return 'V';  // ESP-IDF's Verbose, see LineParser.h
    }
    return 'I';
}

// Same variadic-shim trick as LogRouter.cpp: vprintf_like_t needs an
// actual va_list, which only a variadic call site can produce.
int invokeRawWrite(vprintf_like_t rawWrite, const char* fmt, ...) {
    if (!rawWrite) return 0;
    va_list args;
    va_start(args, fmt);
    int result = rawWrite(fmt, args);
    va_end(args);
    return result;
}

}  // namespace

void ConsoleSink::write(const LogRecord& record, ConsoleFormat format, vprintf_like_t rawWrite) {
    if (format == ConsoleFormat::Jsonl) {
        writeJsonl(record, rawWrite);
    } else {
        writeText(record, rawWrite);
    }
}

void ConsoleSink::writeText(const LogRecord& record, vprintf_like_t rawWrite) {
    char line[kMaxTagLen + kMaxMessageLen + 64];
    if (record.repeatCount > 0) {
        snprintf(line, sizeof(line), "%c (%llu) %s: %s (repeated %ux)\n", levelChar(record.level),
                 static_cast<unsigned long long>(record.uptimeMs), record.tag, record.message,
                 static_cast<unsigned>(record.repeatCount));
    } else {
        snprintf(line, sizeof(line), "%c (%llu) %s: %s\n", levelChar(record.level),
                 static_cast<unsigned long long>(record.uptimeMs), record.tag, record.message);
    }
    invokeRawWrite(rawWrite, "%s", line);
}

void ConsoleSink::writeJsonl(const LogRecord& record, vprintf_like_t rawWrite) {
    CJsonPtr root(cJSON_CreateObject());
    // "ts" == "up" (uptimeMs) for now - no epoch clock until SNTP exists
    // (a later phase). Both fields are kept so consumers that expect a
    // "ts" key don't need a schema change once epoch time lands; only the
    // *value* changes then, not the shape.
    cJSON_AddNumberToObject(root.get(), "ts", static_cast<double>(record.uptimeMs));
    cJSON_AddNumberToObject(root.get(), "up", static_cast<double>(record.uptimeMs));
    cJSON_AddStringToObject(root.get(), "lvl", toString(record.level));
    cJSON_AddStringToObject(root.get(), "tag", record.tag);
    cJSON_AddStringToObject(root.get(), "msg", record.message);
    if (record.repeatCount > 0) {
        cJSON_AddNumberToObject(root.get(), "repeat", record.repeatCount);
    }

    char* jsonStr = cJSON_PrintUnformatted(root.get());
    if (!jsonStr) return;
    invokeRawWrite(rawWrite, "%s\n", jsonStr);
    cJSON_free(jsonStr);
}

}  // namespace croniot::log
