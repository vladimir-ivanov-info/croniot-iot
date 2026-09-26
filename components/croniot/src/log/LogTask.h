#ifndef CRONIOT_LOG_LOGTASK_H
#define CRONIOT_LOG_LOGTASK_H

#include "ConsoleSink.h"
#include "Level.h"

namespace croniot::log {

// Owns the single `croniot_log` FreeRTOS task: the only place that
// actually renders a LogRecord to a sink. Everything upstream (the
// esp_log hook, croniot::log::event()) only ever pushes into NoinitRing -
// see LogRouter.h for why nothing renders synchronously from the hook.
//
// PR7 has exactly one sink (console), so this task's loop is: pop the
// ring, render to console, repeat; poll briefly when the ring is empty
// instead of blocking on a semaphore (a proper wake-up notification is a
// nice-to-have PR7 skips - see LogTask.cpp for why). It also periodically
// flushes a stuck RateLimiter run so a repeat that stopped happening
// doesn't wait forever for a *different* line to release it.
class LogTask {
public:
    // Starts the task if CONFIG_CRONIOT_LOG_ENABLE is set. Idempotent.
    static void start();

    static void setConsoleFormat(ConsoleFormat format);

    // Applied on every drained record before it reaches ConsoleSink - this
    // is what makes `croniot::log::sink(Sink::Console).off()/.level(...)`
    // (see Log.h) actually change what gets rendered, on top of (not
    // instead of) the esp_log_level_set() ceiling Log.cpp maintains: that
    // ceiling stops a line from ever reaching the ring at all, this stops
    // an already-captured line (kept for the RTC store / a future flash
    // sink) from also going to the console.
    static void setConsoleEnabled(bool enabled);
    static void setConsoleLevel(Level level);

private:
    static void run(void* arg);
};

}  // namespace croniot::log

#endif
