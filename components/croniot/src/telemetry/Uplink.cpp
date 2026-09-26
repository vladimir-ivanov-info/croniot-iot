#include "Uplink.h"

#include <cinttypes>

#include "CJsonPtr.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "comm/MessageBus.h"
#include "log/BootCounter.h"
#include "log/Journal.h"
#include "Result.h"
#include "telemetry/BatchEnvelope.h"
#include "telemetry/RetryPolicy.h"
#include "telemetry/UplinkScheduler.h"

namespace croniot::telemetry {

namespace {

constexpr const char* TAG = "Uplink";

#ifdef CONFIG_CRONIOT_UPLINK_TASK_STACK_SIZE
constexpr uint32_t kStackSize = CONFIG_CRONIOT_UPLINK_TASK_STACK_SIZE;
#else
constexpr uint32_t kStackSize = 4096;
#endif

#ifdef CONFIG_CRONIOT_UPLINK_TASK_PRIORITY
constexpr UBaseType_t kPriority = CONFIG_CRONIOT_UPLINK_TASK_PRIORITY;
#else
constexpr UBaseType_t kPriority = 4;
#endif

#ifdef CONFIG_CRONIOT_UPLINK_MAX_BATCH_RECORDS
constexpr size_t kMaxBatchRecords = CONFIG_CRONIOT_UPLINK_MAX_BATCH_RECORDS;
#else
constexpr size_t kMaxBatchRecords = 32;
#endif

#ifdef CONFIG_CRONIOT_UPLINK_MAX_BATCH_BYTES
constexpr size_t kMaxBatchBytes = CONFIG_CRONIOT_UPLINK_MAX_BATCH_BYTES;
#else
constexpr size_t kMaxBatchBytes = 4096;
#endif

#ifdef CONFIG_CRONIOT_UPLINK_ACK_TIMEOUT_MS
constexpr uint64_t kAckTimeoutMs = CONFIG_CRONIOT_UPLINK_ACK_TIMEOUT_MS;
#else
constexpr uint64_t kAckTimeoutMs = 30000;  // plan §5 point 5
#endif

#ifdef CONFIG_CRONIOT_UPLINK_BACKOFF_CAP_MS
constexpr uint64_t kBackoffCapMs = CONFIG_CRONIOT_UPLINK_BACKOFF_CAP_MS;
#else
constexpr uint64_t kBackoffCapMs = 300000;  // 5 min
#endif

#ifdef CONFIG_CRONIOT_UPLINK_MAX_ATTEMPTS
constexpr uint32_t kMaxAttempts = CONFIG_CRONIOT_UPLINK_MAX_ATTEMPTS;
#else
constexpr uint32_t kMaxAttempts = 8;
#endif

// Anti-starvation quota for Stream::Data (plan §5 point 9): once
// Events/Logs have been draining for this long straight, Data gets a
// turn even if the higher-priority streams still have backlog left -
// see UplinkScheduler::pickNext().
constexpr uint64_t kDataStarvationMs = 5 * 60 * 1000;

constexpr TickType_t kPollIntervalTicks = pdMS_TO_TICKS(200);

uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }

// No RNG dependency added for one call site: esp_timer_get_time()'s low
// bits are as good a jitter source as anything else here, and determinism
// isn't a concern on this path the way it is for RetryPolicy's own tests
// (which supply jitter explicitly - see RetryPolicy.h).
double jitterFraction() { return static_cast<double>(esp_timer_get_time() % 1000) / 1000.0 * 0.3; }

size_t index(croniot::log::Stream stream) { return static_cast<size_t>(stream); }

TaskHandle_t g_taskHandle = nullptr;

}  // namespace

Uplink& Uplink::instance() {
    static Uplink uplink;
    return uplink;
}

void Uplink::start() {
    if (started_) return;
    started_ = true;

    croniot::MessageBus::instance().subscribeAck(
        [](const std::string& json) { Uplink::instance().onAck(json); });

    // Birth (plan §4): the online counterpart to the LWT "offline"
    // WifiMqttController::init() already registers on this same status
    // topic. Deliberately minimal for PR12 - `reset_reason`/`fw` need the
    // same ResetCause mapping IncidentRecovery.cpp already has and
    // esp_app_get_description(), neither exposed outside their own
    // translation units yet; left as a documented follow-up rather than
    // duplicating that mapping here.
    CJsonPtr birth(cJSON_CreateObject());
    cJSON_AddNumberToObject(birth.get(), "boot", croniot::log::BootCounter::current());
    char* birthStr = cJSON_PrintUnformatted(birth.get());
    croniot::MessageBus::instance().publishStatus(birthStr, /*retain=*/true);
    cJSON_free(birthStr);

    xTaskCreate(&Uplink::run, "croniot_uplink", kStackSize, nullptr, kPriority, &g_taskHandle);
}

void Uplink::run(void*) {
    for (;;) {
        Uplink::instance().serviceOnce();
        vTaskDelay(kPollIntervalTicks);
    }
}

void Uplink::onAck(const std::string& json) {
    CJsonPtr root(cJSON_Parse(json.c_str()));
    if (!root) {
        ESP_LOGW(TAG, "malformed ack payload: %s", json.c_str());
        return;
    }

    cJSON* streamField = cJSON_GetObjectItem(root.get(), "stream");
    cJSON* upToSeqField = cJSON_GetObjectItem(root.get(), "upToSeq");
    if (!cJSON_IsString(streamField) || !cJSON_IsNumber(upToSeqField)) {
        ESP_LOGW(TAG, "ack missing stream/upToSeq: %s", json.c_str());
        return;
    }

    croniot::log::Stream stream;
    std::string streamName = streamField->valuestring;
    if (streamName == "logs") {
        stream = croniot::log::Stream::Logs;
    } else if (streamName == "events") {
        stream = croniot::log::Stream::Events;
    } else if (streamName == "data") {
        stream = croniot::log::Stream::Data;
    } else {
        ESP_LOGW(TAG, "ack for unknown stream '%s'", streamName.c_str());
        return;
    }

    uint32_t upToSeq = static_cast<uint32_t>(upToSeqField->valuedouble);
    croniot::log::Journal::instance().ack(stream, upToSeq);

    // Simplification, documented: any ack for a stream clears its
    // in-flight slot outright, even a partial one (upToSeq short of
    // firstSeq+count-1) - the next serviceOnce() just re-reads from
    // Journal::firstUnackedSeq() and re-batches whatever's still
    // outstanding, rather than tracking partial-ack remainders here too.
    InFlightBatch& slot = inFlight_[index(stream)];
    if (slot.active && upToSeq + 1 >= slot.firstSeq) {
        slot.active = false;
    }
}

void Uplink::serviceOnce() {
    std::vector<StreamBacklog> backlogs;
    uint64_t now = nowMs();
    for (auto stream : {croniot::log::Stream::Events, croniot::log::Stream::Logs,
                         croniot::log::Stream::Data}) {
        InFlightBatch& slot = inFlight_[index(stream)];
        uint32_t resumeSeq = slot.active ? slot.firstSeq
                                          : croniot::log::Journal::instance().firstUnackedSeq(stream);
        bool hasPending = croniot::log::Journal::instance().nextSeq(stream) > resumeSeq;

        uint64_t& pendingSince = pendingSinceMs_[index(stream)];
        if (hasPending) {
            if (pendingSince == 0) pendingSince = now;
        } else {
            pendingSince = 0;
        }
        uint64_t ageMs = hasPending ? now - pendingSince : 0;

        backlogs.push_back({stream, hasPending, slot.active, ageMs});
    }

    auto pick = UplinkScheduler::pickNext(backlogs, kDataStarvationMs);
    if (!pick) return;

    InFlightBatch& slot = inFlight_[index(*pick)];

    if (slot.active) {
        if (now < slot.nextSendDueMs) return;  // waiting on ack or backoff window

        ++slot.attempt;
        if (RetryPolicy::shouldGiveUp(slot.attempt, kMaxAttempts)) {
            ESP_LOGW(TAG, "giving up on stream=%d firstSeq=%" PRIu32 " after %" PRIu32
                          " attempts; link degraded, data stays in the journal",
                     static_cast<int>(*pick), slot.firstSeq, slot.attempt);
            slot.active = false;  // stop retrying THIS attempt; the unacked data is untouched in Journal and will be re-picked up on a later serviceOnce() pass
            return;
        }

        sendBatch(*pick, slot.firstSeq, slot.count, slot.frames);
        slot.nextSendDueMs =
            now + RetryPolicy::nextDelayMs(slot.attempt, kAckTimeoutMs, kBackoffCapMs, jitterFraction());
        return;
    }

    uint32_t fromSeq = croniot::log::Journal::instance().firstUnackedSeq(*pick);
    auto batch =
        croniot::log::Journal::instance().readFrom(*pick, fromSeq, kMaxBatchBytes, kMaxBatchRecords);
    if (!batch) return;

    slot.active = true;
    slot.firstSeq = batch->firstSeq;
    slot.count = batch->count;
    slot.attempt = 1;
    slot.frames = std::move(batch->frames);

    sendBatch(*pick, slot.firstSeq, slot.count, slot.frames);
    slot.nextSendDueMs = now + kAckTimeoutMs;
}

bool Uplink::sendBatch(croniot::log::Stream stream, uint32_t firstSeq, uint32_t count,
                       const std::vector<uint8_t>& frames) {
    auto envelope =
        encodeBatchEnvelope(croniot::log::BootCounter::current(), stream, firstSeq, count, frames);
    std::string payload(envelope.begin(), envelope.end());

    Result result(false, "unknown stream");
    if (stream == croniot::log::Stream::Logs) {
        result = croniot::MessageBus::instance().publishLogBatch(payload);
    } else if (stream == croniot::log::Stream::Events) {
        result = croniot::MessageBus::instance().publishDeviceEvent(payload);
    } else {
        // Stream::Data (plan §7.2/§12.6): sensor batches, encoded by
        // Sensors/SensorBatchEncoder.h and appended via Journal::
        // appendRaw() - the wire topic Uplink.h's class comment
        // documented as not existing until this batch landed.
        result = croniot::MessageBus::instance().publishSensorBatch(payload);
    }

    if (!result.success) {
        ESP_LOGW(TAG, "publish failed for stream=%d firstSeq=%" PRIu32 " count=%" PRIu32 ": %s",
                 static_cast<int>(stream), firstSeq, count, result.message.c_str());
    }
    return result.success;
}

}  // namespace croniot::telemetry
