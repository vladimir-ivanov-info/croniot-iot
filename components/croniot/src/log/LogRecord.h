#ifndef CRONIOT_LOG_LOGRECORD_H
#define CRONIOT_LOG_LOGRECORD_H

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "Level.h"

namespace croniot::log {

// Fixed-size, trivially-copyable record: no heap pointers, so the exact
// same layout can sit in the RAM ring, get copied into `.noinit` for the
// crash black box (Fase 2), and be handed to CborWriter - without ever
// needing a different representation for "the same event" depending on
// where it currently lives.
//
// Deliberately has NO default member initializers, so its default
// constructor stays trivial (the compiler emits no init code for it at
// all). That's not a style nit: a global/array instance of a type with a
// *non*-trivial default constructor gets that constructor invoked at
// startup regardless of which linker section it's placed in, which would
// silently wipe the `.noinit` black box on every boot - exactly the crash
// data it exists to preserve. Any call site that needs a fresh, zeroed
// record must say so explicitly with `LogRecord record{};`.
inline constexpr size_t kMaxTagLen = 15;
inline constexpr size_t kMaxMessageLen = 200;

struct LogRecord {
    uint32_t seq;
    uint64_t uptimeMs;
    Level level;
    // >0 only on a record synthesized by RateLimiter to summarize a run of
    // identical lines it collapsed (see RateLimiter.h). 0 on every other
    // record - this is compression bookkeeping, not part of the wire format.
    uint16_t repeatCount;
    char tag[kMaxTagLen + 1];
    char message[kMaxMessageLen + 1];

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

// Enforced at compile time, not just by comment: if this ever fails, a
// global/`.noinit` instance of LogRecord would get silently re-initialized
// by a startup constructor call, defeating the crash black box.
static_assert(std::is_trivial_v<LogRecord>, "LogRecord must stay trivial for .noinit placement");

}  // namespace croniot::log

#endif
