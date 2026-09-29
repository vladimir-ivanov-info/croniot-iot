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

// `vprintf_like_t` is already `int(*)(const char*, va_list)` - the exact
// shape of vprintfHook's own `args` parameter, so forwarding it to
// `previousHook_` needs nothing more than a direct call.
//
// A previous version of this file routed that call through a variadic
// shim (`invokeHook(hook, fmt, ...)`, passing `args` itself as "a"
// variadic argument, then doing a fresh `va_start` inside to try to
// rebuild a va_list from it). That doesn't reconstruct the original
// arguments: a va_list has no portable representation as "one more
// variadic argument" to a *different* variadic function, so the va_start
// inside the shim read whatever bytes happened to follow on the stack -
// garbage, not the real (level, uptime, tag, ...) arguments ESP-IDF's
// LOG_FORMAT expects. It happened to go unexercised until a WiFi-driver
// log line that doesn't match croniot::log's v1 shape hit the fallback
// path for the first time on real hardware, and crashed with a Guru
// Meditation Load access fault (strlen() inside vfprintf, called on a
// garbage "tag" pointer reconstructed from the shim's bogus va_list).
int callPreviousHook(vprintf_like_t hook, const char* fmt, va_list args) {
    return hook ? hook(fmt, args) : 0;
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
        return callPreviousHook(self.previousHook_, fmt, args);
    }
    size_t clampedLen = static_cast<size_t>(len) < sizeof(buf) ? static_cast<size_t>(len) : sizeof(buf) - 1;

    // parseEspLogLineFast()/redactInPlace() (not parseEspLogLine()/redact())
    // deliberately: this hook runs on whatever task called ESP_LOGx,
    // including ESP-IDF's own small internal tasks (WiFi driver, lwIP...),
    // and the std::string-based versions do several heap allocations per
    // call. That combination (extra stack depth + heap churn on a task
    // whose stack ESP-IDF sized for its own lightweight default vprintf)
    // was the confirmed root cause of a real on-device crash: garbled log
    // output during WiFi init immediately followed by a Guru Meditation
    // Load access fault in the WiFi driver task.
    LogRecord record{};
    if (!parseEspLogLineFast(buf, clampedLen, record)) {
        // Not our v1 "<L> (<uptime>) <tag>: <msg>" shape (e.g. a raw
        // printf from third-party code) - pass it through unchanged
        // rather than dropping it. `args` hasn't been consumed yet (only
        // `argsCopy` was), so it's still valid for this call.
        return callPreviousHook(self.previousHook_, fmt, args);
    }
    self.redactor_.redactInPlace(record.message, sizeof(record.message));
    record.repeatCount = 0;

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
