#ifndef CRONIOT_HEALTH_HEALTHREPORT_H
#define CRONIOT_HEALTH_HEALTHREPORT_H

#include <array>
#include <cstdint>
#include <string>

namespace croniot::health {

// Plan §11.4 (Fase 3): "informe de salud periodico como evento health:
// heap libre, minimo y mayor bloque, stack HWM de las tareas, uptime,
// RSSI, reconexiones, logs perdidos y uso de flash" plus §5 point 13's
// uplink signals (backlogBytes/backlogAge/resends). Rendered here as
// compact key=value pairs rather than JSON: this travels through
// croniot::log::event(), whose LogRecord::message caps at
// kMaxMessageLen (200 bytes, see LogRecord.h) and silently truncates
// past that - so the encoding has to be provably short, not just
// usually short. Two events instead of one ("health"/"health_storage")
// keeps each comfortably under that cap even at worst-case field
// widths (see HealthReportTest's boundary case) and keeps each event
// independently useful (mirrors the plan's "no loguees lo que puedes
// contar": separate concerns get separate counters/events).
//
// Pure formatting, no ESP-IDF dependency - every input is a plain value
// the platform layer (health/Health.cpp) is responsible for collecting
// from esp_get_free_heap_size()/Journal/Uplink/Counters/WiFi.

// Sentinel for "no reading available" (WiFi not connected, or a
// partition not mounted) - kept as a real out-of-band value rather than
// std::optional so the encoder stays trivially host-testable with plain
// structs, matching this codebase's existing pure-logic style
// (RetryPolicy, UplinkScheduler).
inline constexpr int8_t kNoRssi = INT8_MIN;
inline constexpr int kNoPercent = -1;

struct MemNetInputs {
    uint32_t freeHeapBytes = 0;
    uint32_t largestFreeBlockBytes = 0;
    uint64_t uptimeSeconds = 0;
    int8_t rssi = kNoRssi;  // kNoRssi if not connected
    uint32_t wifiReconnects = 0;
    uint32_t logsDropped = 0;
};

struct StorageInputs {
    int journalFreePercent = kNoPercent;   // 0-100, kNoPercent if not mounted
    int archiveFreePercent = kNoPercent;
    int journalWearPercent = kNoPercent;   // 0-100 (clamped), kNoPercent if unknown
    int archiveWearPercent = kNoPercent;
    // Indexed by croniot::log::Stream (Logs=0, Events=1, Data=2).
    std::array<uint32_t, 3> backlogCount{};
    std::array<uint32_t, 3> resendAttempts{};
};

// "heap=<B> blk=<B> up=<s> rssi=<dBm|na> recon=<N> drop=<N>"
std::string encodeMemNet(const MemNetInputs& in);

// "jfree=<pct|na> afree=<pct|na> jwear=<pct|na> awear=<pct|na>
//  lbl=<N> ebl=<N> dbl=<N> lrs=<N> ers=<N> drs=<N>"
std::string encodeStorage(const StorageInputs& in);

}  // namespace croniot::health

#endif
