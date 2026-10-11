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

    // Length-bounded counterparts, for callers holding a (pointer, length)
    // span into a buffer that isn't NUL-terminated at the span's end (e.g.
    // a slice of a stack buffer) - avoids the strlen() a `const char*`
    // overload would need, and avoids the caller having to allocate a
    // temporary just to NUL-terminate first. Used by LineParser's
    // allocation-free parseEspLogLineFast().
    void setTag(const char* value, size_t len) { copyTruncated(tag, sizeof(tag), value, len); }
    void setMessage(const char* value, size_t len) { copyTruncated(message, sizeof(message), value, len); }

private:
    static void copyTruncated(char* dst, size_t dstSize, const char* src) {
        if (!src) {
            dst[0] = '\0';
            return;
        }
        copyTruncated(dst, dstSize, src, std::strlen(src));
    }

    static void copyTruncated(char* dst, size_t dstSize, const char* src, size_t srcLen) {
        if (!src) {
            dst[0] = '\0';
            return;
        }
        size_t len = srcLen > dstSize - 1 ? dstSize - 1 : srcLen;
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
