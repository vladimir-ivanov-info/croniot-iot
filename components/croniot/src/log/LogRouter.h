#ifndef CRONIOT_LOG_LOGROUTER_H
#define CRONIOT_LOG_LOGROUTER_H

#include <cstdint>
#include <string>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "LogRecord.h"
#include "RateLimiter.h"
#include "Redactor.h"

namespace croniot::log {

// RAII guard for the recursion-prevention extension point sinks need:
// a sink's own internals sometimes call ESP_LOGx (a flash driver logging
// a write failure, an MQTT client logging a disconnect, ...), which would
// otherwise re-enter this exact pipeline from inside a sink write. Not
// fully exercised in PR7 - the only sink today is the console, and console
// writes don't themselves call ESP_LOGx - but flash (PR9) and MQTT (PR12)
// need it, so the extension point is built now. thread_local because the
// drain task is the only place that holds it, but keeping it per-task
// rather than global avoids any cross-task false positive if a future
// sink write happens from more than one task.
class SinkWriteGuard {
public:
    SinkWriteGuard();
    ~SinkWriteGuard();

    SinkWriteGuard(const SinkWriteGuard&) = delete;
    SinkWriteGuard& operator=(const SinkWriteGuard&) = delete;

    // True if a SinkWriteGuard was already active on this task before this
    // one was constructed - i.e. this call is a re-entrant one and should
    // skip writing again.
    bool reentrant() const { return wasAlreadyActive_; }

private:
    bool wasAlreadyActive_;
};

// Installs the esp_log_set_vprintf() hook and turns every line ESP_LOGx
// produces into a LogRecord flowing through: parse -> redact -> rate
// limiter -> NoinitRing (+ RtcCriticalStore for Level::Error). Nothing in
// this path blocks on flash/SD/network or renders to any sink directly -
// see LogTask.h for why rendering lives in the drain task instead, even
// for the default/console-only profile.
class LogRouter {
public:
    static LogRouter& instance();

    // Idempotent: safe to call more than once. Also runs NoinitRing::init()
    // and RtcCriticalStore::init() (both idempotent-safe too).
    void install();

    void registerSecret(const std::string& secret) { redactor_.registerSecret(secret); }

    // The default/previous vprintf hook esp_log_set_vprintf() returned when
    // this router installed its own - exposed so ConsoleSink can chain to
    // the real UART writer instead of reimplementing it (see ConsoleSink.h).
    vprintf_like_t previousHook() const { return previousHook_; }

    // Builds a LogRecord tagged as an event (see croniot::log::event() in
    // Log.h) and pushes it straight into the ring, bypassing the rate
    // limiter entirely - events are never collapsed/rate-limited, they're
    // rare and always intentional by construction.
    void pushEvent(const LogRecord& record);

    // Releases a still-open RateLimiter run (see RateLimiter::flushPending)
    // and pushes the resulting summary record into the ring, if there was
    // one. Called periodically by LogTask so a stuck repeat isn't held
    // forever waiting for a different line to arrive.
    void flushRateLimiterPending();

private:
    LogRouter() = default;

    static int vprintfHook(const char* fmt, va_list args);

    vprintf_like_t previousHook_ = nullptr;
    Redactor redactor_;
    RateLimiter rateLimiter_;
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    uint32_t nextSeq_ = 0;
    bool installed_ = false;
};

}  // namespace croniot::log

#endif
