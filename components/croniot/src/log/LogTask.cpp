#include "LogTask.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "LogRouter.h"
#include "NoinitRing.h"

namespace croniot::log {

namespace {

#ifdef CONFIG_CRONIOT_LOG_TASK_STACK_SIZE
constexpr uint32_t kStackSize = CONFIG_CRONIOT_LOG_TASK_STACK_SIZE;
#else
constexpr uint32_t kStackSize = 4096;
#endif

#ifdef CONFIG_CRONIOT_LOG_TASK_PRIORITY
constexpr UBaseType_t kPriority = CONFIG_CRONIOT_LOG_TASK_PRIORITY;
#else
constexpr UBaseType_t kPriority = 1;
#endif

// Empty-ring poll interval and the RateLimiter flush period. A real
// notification/semaphore wake-up (LogRouter signalling the task the
// instant it pushes) would shave this latency to ~0, but a short poll is
// simple, correct, and - at 20 ms - not perceptibly different from
// synchronous console output to someone watching `idf.py monitor`. This
// is the PR7 judgment call: effort (a wake primitive threaded through the
// hook, which must stay allocation/block-free) vs. value (shaving single-
// digit milliseconds nobody will notice on a serial console).
constexpr TickType_t kPollIntervalTicks = pdMS_TO_TICKS(20);
constexpr TickType_t kFlushIntervalTicks = pdMS_TO_TICKS(3000);

ConsoleFormat g_consoleFormat = ConsoleFormat::Text;
bool g_consoleEnabled = true;
Level g_consoleLevel = Level::Info;
TaskHandle_t g_taskHandle = nullptr;

}  // namespace

void LogTask::setConsoleFormat(ConsoleFormat format) { g_consoleFormat = format; }
void LogTask::setConsoleEnabled(bool enabled) { g_consoleEnabled = enabled; }
void LogTask::setConsoleLevel(Level level) { g_consoleLevel = level; }

void LogTask::start() {
    if (g_taskHandle != nullptr) return;  // idempotent
    xTaskCreate(&LogTask::run, "croniot_log", kStackSize, nullptr, kPriority, &g_taskHandle);
}

void LogTask::run(void*) {
    TickType_t lastFlush = xTaskGetTickCount();

    for (;;) {
        auto record = NoinitRing::pop();
        if (record) {
            if (g_consoleEnabled && meetsThreshold(record->level, g_consoleLevel)) {
                SinkWriteGuard guard;
                if (!guard.reentrant()) {
                    ConsoleSink::write(*record, g_consoleFormat, LogRouter::instance().previousHook());
                }
            }
        } else {
            vTaskDelay(kPollIntervalTicks);
        }

        TickType_t now = xTaskGetTickCount();
        if (now - lastFlush >= kFlushIntervalTicks) {
            LogRouter::instance().flushRateLimiterPending();
            lastFlush = now;
        }
    }
}

}  // namespace croniot::log
