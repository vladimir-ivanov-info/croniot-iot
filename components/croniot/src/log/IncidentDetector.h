#ifndef CRONIOT_LOG_INCIDENTDETECTOR_H
#define CRONIOT_LOG_INCIDENTDETECTOR_H

#include <cstdint>

namespace croniot::log {

// Mirrors esp_reset_reason_t (esp_system.h) but as a plain enum this file
// can be tested against without pulling in ESP-IDF - the platform glue
// (IncidentRecovery.cpp) is the only place that maps one to the other.
enum class ResetCause {
    Unknown,
    PowerOn,
    ExternalPin,
    SoftwareRestart,   // esp_restart()
    DeepSleepWake,
    Panic,
    InterruptWdt,
    TaskWdt,
    OtherWdt,
    Brownout,
    SdioReset,
    Other
};

// True for the reset flavours HP SRAM / RTC memory actually survive
// (plan §3.7's table: "esp_restart(), panic, abort(), watchdog: Sí") and
// that therefore justify treating leftover .noinit/RTC data as evidence
// of a real incident rather than stale garbage from long before a normal
// power-on. `PowerOn`/`Brownout`/`DeepSleepWake` do NOT retain HP SRAM
// (deep sleep) or happen with cleared memory (power-on/brownout) - by
// the time this runs, NoinitRing::init()/RtcCriticalStore::init() will
// already have reset themselves via their own magic/CRC check in those
// cases, so this function is a second, independent signal, not the only
// one: an incident is only raised when BOTH agree.
inline bool isCrashLikeReset(ResetCause cause) {
    switch (cause) {
        case ResetCause::SoftwareRestart:
        case ResetCause::Panic:
        case ResetCause::InterruptWdt:
        case ResetCause::TaskWdt:
        case ResetCause::OtherWdt:
            return true;
        default:
            return false;
    }
}

struct IncidentSummary {
    ResetCause cause = ResetCause::Unknown;
    uint32_t recoveredRingRecords = 0;   // still-unread NoinitRing entries at boot
    uint32_t recoveredRtcRecords = 0;    // RtcCriticalStore::count() at boot
    bool coredumpPresent = false;
};

// An incident is worth raising only if the reset looks crash-like AND
// there's actual evidence to report - a clean esp_restart() (e.g. after
// an OTA) with an empty ring and no coredump is not an incident, it's
// routine. This is what stops every deliberate reboot from generating a
// spurious "incident_previous_boot" event.
inline bool shouldRaiseIncident(const IncidentSummary& summary) {
    return isCrashLikeReset(summary.cause) &&
           (summary.recoveredRingRecords > 0 || summary.recoveredRtcRecords > 0 ||
            summary.coredumpPresent);
}

}  // namespace croniot::log

#endif
