#ifndef CRONIOT_LOG_LOGRECORD_H
#define CRONIOT_LOG_LOGRECORD_H

#include <cstdint>
#include <cstring>

#include "Level.h"

namespace croniot::log {

// Fixed-size, trivially-copyable record: no heap pointers, so the exact
// same type can sit in the RAM ring, get memcpy'd into `.noinit` for the
// crash black box (Fase 2), and be handed to CborWriter - without ever
// needing a different representation for "the same event" depending on
// where it currently lives.
inline constexpr size_t kMaxTagLen = 15;
inline constexpr size_t kMaxMessageLen = 200;

struct LogRecord {
    uint32_t seq = 0;
    uint64_t uptimeMs = 0;
    Level level = Level::Info;
    // >0 only on a record synthesized by RateLimiter to summarize a run of
    // identical lines it collapsed (see RateLimiter.h). 0 on every other
    // record - this is compression bookkeeping, not part of the wire format.
    uint16_t repeatCount = 0;
    char tag[kMaxTagLen + 1] = {};
    char message[kMaxMessageLen + 1] = {};

    void setTag(const char* value) { copyTruncated(tag, sizeof(tag), value); }
    void setMessage(const char* value) { copyTruncated(message, sizeof(message), value); }

private:
    static void copyTruncated(char* dst, size_t dstSize, const char* src) {
        if (!src) {
            dst[0] = '\0';
            return;
        }
        size_t len = std::strlen(src);
        if (len > dstSize - 1) len = dstSize - 1;
        std::memcpy(dst, src, len);
        dst[len] = '\0';
    }
};

}  // namespace croniot::log

#endif
