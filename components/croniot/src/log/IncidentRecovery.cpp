#include "IncidentRecovery.h"

#include <cstdio>

#include "esp_core_dump.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "IncidentDetector.h"
#include "Journal.h"
#include "Level.h"
#include "LogRecord.h"
#include "LogRouter.h"
#include "NoinitRing.h"
#include "RtcCriticalStore.h"

namespace croniot::log {

namespace {

ResetCause toResetCause(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return ResetCause::PowerOn;
        case ESP_RST_EXT: return ResetCause::ExternalPin;
        case ESP_RST_SW: return ResetCause::SoftwareRestart;
        case ESP_RST_DEEPSLEEP: return ResetCause::DeepSleepWake;
        case ESP_RST_PANIC: return ResetCause::Panic;
        case ESP_RST_INT_WDT: return ResetCause::InterruptWdt;
        case ESP_RST_TASK_WDT: return ResetCause::TaskWdt;
        case ESP_RST_WDT: return ResetCause::OtherWdt;
        case ESP_RST_BROWNOUT: return ResetCause::Brownout;
        case ESP_RST_SDIO: return ResetCause::SdioReset;
        default: return ResetCause::Unknown;
    }
}

const char* toString(ResetCause cause) {
    switch (cause) {
        case ResetCause::PowerOn: return "power_on";
        case ResetCause::ExternalPin: return "external_pin";
        case ResetCause::SoftwareRestart: return "software_restart";
        case ResetCause::DeepSleepWake: return "deep_sleep_wake";
        case ResetCause::Panic: return "panic";
        case ResetCause::InterruptWdt: return "interrupt_wdt";
        case ResetCause::TaskWdt: return "task_wdt";
        case ResetCause::OtherWdt: return "other_wdt";
        case ResetCause::Brownout: return "brownout";
        case ResetCause::SdioReset: return "sdio_reset";
        case ResetCause::Other: return "other";
        case ResetCause::Unknown: return "unknown";
    }
    return "unknown";
}

}  // namespace

void IncidentRecovery::run() {
    IncidentSummary summary;
    summary.cause = toResetCause(esp_reset_reason());
    summary.recoveredRingRecords = NoinitRing::size();
    summary.recoveredRtcRecords = RtcCriticalStore::count();
    summary.coredumpPresent = esp_core_dump_image_check() == ESP_OK;

    if (!shouldRaiseIncident(summary)) return;

    char message[196];
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    esp_core_dump_summary_t coreSummary;
    bool haveCoreSummary =
        summary.coredumpPresent && esp_core_dump_get_summary(&coreSummary) == ESP_OK;
    if (haveCoreSummary) {
        std::snprintf(message, sizeof(message),
                      "cause=%s ring_records=%u rtc_records=%u coredump=true "
                      "task=%s pc=0x%08x",
                      toString(summary.cause), summary.recoveredRingRecords,
                      summary.recoveredRtcRecords, coreSummary.exc_task, coreSummary.exc_pc);
    } else
#endif
    {
        std::snprintf(message, sizeof(message), "cause=%s ring_records=%u rtc_records=%u coredump=%s",
                      toString(summary.cause), summary.recoveredRingRecords,
                      summary.recoveredRtcRecords, summary.coredumpPresent ? "true" : "false");
    }

    // Deliberately built and pushed by hand rather than via
    // croniot::log::event() (Log.h): that function lives in the same
    // translation unit that calls IncidentRecovery::run(), and routing
    // through it here would mean Log.cpp depending on IncidentRecovery.h
    // depending back on Log.h - LogRouter::pushEvent() and
    // Journal::appendEvent() are the two things event() itself calls, so
    // calling them directly here gets the same effect (console/RTC via
    // the ring, durable via the Events stream) without the cycle.
    LogRecord record{};
    record.level = Level::Error;
    record.setTag("incident_previous_boot");
    record.setMessage(message);
    LogRouter::instance().pushEvent(record);
    Journal::instance().appendEvent(record);
}

}  // namespace croniot::log
