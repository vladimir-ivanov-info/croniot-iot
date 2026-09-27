#include "Health.h"

#include <cinttypes>
#include <cstdio>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "HealthReport.h"
#include "log/Counters.h"
#include "log/Journal.h"
#include "log/Log.h"
#include "log/NoinitRing.h"
#include "telemetry/Uplink.h"

namespace croniot::health {

namespace {

constexpr const char* TAG = "Health";

#ifdef CONFIG_CRONIOT_HEALTH_INTERVAL_SEC
constexpr uint32_t kIntervalSec = CONFIG_CRONIOT_HEALTH_INTERVAL_SEC;
#else
constexpr uint32_t kIntervalSec = 300;
#endif

#ifdef CONFIG_CRONIOT_HEALTH_TASK_STACK_SIZE
constexpr uint32_t kStackSize = CONFIG_CRONIOT_HEALTH_TASK_STACK_SIZE;
#else
constexpr uint32_t kStackSize = 4096;
#endif

#ifdef CONFIG_CRONIOT_HEALTH_TASK_PRIORITY
constexpr UBaseType_t kPriority = CONFIG_CRONIOT_HEALTH_TASK_PRIORITY;
#else
constexpr UBaseType_t kPriority = 1;
#endif

#ifdef CONFIG_CRONIOT_LOG_FLASH_ENDURANCE_CYCLES
constexpr uint32_t kEnduranceCycles = CONFIG_CRONIOT_LOG_FLASH_ENDURANCE_CYCLES;
#else
constexpr uint32_t kEnduranceCycles = 100000;
#endif

// Journal::estimatedWearPercent() returns -1.0 for "partition not
// mounted" (see Journal.h) - everything else is clamped into [0,100].
int clampWearPercent(double pct) {
    if (pct < 0) return kNoPercent;
    return static_cast<int>(pct > 100.0 ? 100.0 : pct);
}

}  // namespace

Health& Health::instance() {
    static Health health;
    return health;
}

void Health::start() {
#if CONFIG_CRONIOT_HEALTH_ENABLE
    if (started_) return;
    started_ = true;
    xTaskCreate(&Health::run, "croniot_health", kStackSize, nullptr, kPriority, nullptr);
#endif
}

void Health::registerMetric(const std::string& name, std::function<double()> valueFn) {
    customMetrics_.emplace_back(name, std::move(valueFn));
}

void Health::run(void*) {
    for (;;) {
        Health::instance().reportOnce();
        vTaskDelay(pdMS_TO_TICKS(kIntervalSec * 1000));
    }
}

void Health::reportOnce() {
    MemNetInputs memNet;
    memNet.freeHeapBytes = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    memNet.largestFreeBlockBytes = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    memNet.uptimeSeconds = static_cast<uint64_t>(esp_timer_get_time() / 1000000);

    wifi_ap_record_t apInfo{};
    memNet.rssi = esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK ? apInfo.rssi : kNoRssi;
    memNet.wifiReconnects = croniot::log::Counters::instance().get("wifi_reconnect");
    memNet.logsDropped = croniot::log::NoinitRing::droppedCount();

    croniot::log::event("health", croniot::log::Level::Info,
                         {{"m", encodeMemNet(memNet)}});

    StorageInputs storage;
    using croniot::log::Stream;
    auto journalUsage = croniot::log::Journal::instance().partitionUsage(Stream::Logs);
    auto archiveUsage = croniot::log::Journal::instance().partitionUsage(Stream::Data);
    if (journalUsage) {
        storage.journalFreePercent = static_cast<int>(
            journalUsage->totalBytes == 0
                ? 0
                : 100 - (journalUsage->usedBytes * 100 / journalUsage->totalBytes));
    }
    if (archiveUsage) {
        storage.archiveFreePercent = static_cast<int>(
            archiveUsage->totalBytes == 0
                ? 0
                : 100 - (archiveUsage->usedBytes * 100 / archiveUsage->totalBytes));
    }
    storage.journalWearPercent = clampWearPercent(
        croniot::log::Journal::instance().estimatedWearPercent(Stream::Logs, kEnduranceCycles));
    storage.archiveWearPercent = clampWearPercent(
        croniot::log::Journal::instance().estimatedWearPercent(Stream::Data, kEnduranceCycles));

    for (Stream s : {Stream::Logs, Stream::Events, Stream::Data}) {
        size_t i = static_cast<size_t>(s);
        uint32_t next = croniot::log::Journal::instance().nextSeq(s);
        uint32_t firstUnacked = croniot::log::Journal::instance().firstUnackedSeq(s);
        storage.backlogCount[i] = next > firstUnacked ? next - firstUnacked : 0;
        storage.resendAttempts[i] = croniot::telemetry::Uplink::instance().currentAttempt(s);
    }

    croniot::log::event("health_storage", croniot::log::Level::Info,
                         {{"m", encodeStorage(storage)}});

    if (!customMetrics_.empty()) {
        std::string message;
        bool first = true;
        for (auto& metric : customMetrics_) {
            if (!first) message += ' ';
            first = false;
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s=%.3f", metric.first.c_str(), metric.second());
            message += buf;
        }
        // Not truncation-proofed the way encodeMemNet/encodeStorage are:
        // this is a project's own metric set, unbounded in count by
        // design (registerMetric() takes arbitrary names), so a project
        // that registers many metrics is responsible for keeping the
        // total under LogRecord's 200-byte cap itself.
        croniot::log::event("health_custom", croniot::log::Level::Info, {{"m", message}});
    }
}

}  // namespace croniot::health
