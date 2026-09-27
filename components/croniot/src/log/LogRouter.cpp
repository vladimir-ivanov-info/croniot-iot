#include "LogRouter.h"

#include <cstdarg>
#include <cstdio>

#include "LineParser.h"
#include "NoinitRing.h"
#include "RtcCriticalStore.h"

namespace croniot::log {

namespace {
thread_local bool g_sinkWriteActive = false;

// esp_log's own line buffer is 128 bytes by default (CONFIG_LOG_MAX_...
// doesn't exist pre-v5.4-style; IDF just fits everything the LOG_FORMAT
// macro assembles into whatever vsnprintf produces). 256 gives headroom
// without needing the heap - this is a stack buffer, never allocated.
constexpr size_t kLineBufferSize = 256;

// Small variadic shim: vprintf_like_t takes (const char*, va_list), and
// the only way to hand a hook a *new* va_list from non-variadic code is
// through an actual variadic call site.
int invokeHook(vprintf_like_t hook, const char* fmt, ...) {
    if (!hook) return 0;
    va_list args;
    va_start(args, fmt);
    int result = hook(fmt, args);
    va_end(args);
    return result;
}
}  // namespace

SinkWriteGuard::SinkWriteGuard() : wasAlreadyActive_(g_sinkWriteActive) { g_sinkWriteActive = true; }

SinkWriteGuard::~SinkWriteGuard() { g_sinkWriteActive = wasAlreadyActive_; }

LogRouter& LogRouter::instance() {
    static LogRouter router;
    return router;
}

void LogRouter::install() {
    if (installed_) return;
    NoinitRing::init();
    RtcCriticalStore::init();
    previousHook_ = esp_log_set_vprintf(&LogRouter::vprintfHook);
    installed_ = true;
}

int LogRouter::vprintfHook(const char* fmt, va_list args) {
    auto& self = instance();

    char buf[kLineBufferSize];
    va_list argsCopy;
    va_copy(argsCopy, args);
    int len = vsnprintf(buf, sizeof(buf), fmt, argsCopy);
    va_end(argsCopy);

    if (len < 0) {
        // Formatting itself failed: nothing sane to parse or redact, fall
        // through to whatever handled log lines before us.
        return invokeHook(self.previousHook_, fmt, args);
    }

    ParsedLine parsed = parseEspLogLine(std::string(buf, static_cast<size_t>(len) < sizeof(buf) ? len : sizeof(buf) - 1));
    if (!parsed.valid) {
        // Not our v1 "<L> (<uptime>) <tag>: <msg>" shape (e.g. a raw
        // printf from third-party code) - pass it through unchanged
        // rather than dropping it. `args` hasn't been consumed yet (only
        // `argsCopy` was), so it's still valid for this call.
        return invokeHook(self.previousHook_, fmt, args);
    }

    std::string redactedMessage = self.redactor_.redact(parsed.message);

    LogRecord record{};
    record.uptimeMs = parsed.uptimeMs;
    record.level = parsed.level;
    record.repeatCount = 0;
    record.setTag(parsed.tag.c_str());
    record.setMessage(redactedMessage.c_str());

    portENTER_CRITICAL(&self.lock_);
    record.seq = self.nextSeq_++;
    RateLimiter::FeedResult feedResult = self.rateLimiter_.feed(record);
    if (feedResult.pendingFlush) {
        NoinitRing::push(*feedResult.pendingFlush);
    }
    if (feedResult.emitNow) {
        NoinitRing::push(record);
        if (record.level == Level::Error) {
            RtcCriticalStore::push(record);
        }
    }
    portEXIT_CRITICAL(&self.lock_);

    return len;
}

void LogRouter::pushEvent(const LogRecord& record) {
    portENTER_CRITICAL(&lock_);
    LogRecord stamped = record;
    stamped.seq = nextSeq_++;
    // Events are never rate-limited - they're rare (call-site opt-in) and
    // always semantically distinct, so RateLimiter::feed is skipped
    // entirely (see Log.h's event() contract).
    NoinitRing::push(stamped);
    RtcCriticalStore::push(stamped);
    portEXIT_CRITICAL(&lock_);
}

void LogRouter::flushRateLimiterPending() {
    portENTER_CRITICAL(&lock_);
    auto flushed = rateLimiter_.flushPending();
    if (flushed) {
        NoinitRing::push(*flushed);
    }
    portEXIT_CRITICAL(&lock_);
}

}  // namespace croniot::log
