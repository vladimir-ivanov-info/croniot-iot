#ifndef CRONIOT_LOG_RTCCRITICALSTORE_H
#define CRONIOT_LOG_RTCCRITICALSTORE_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include "sdkconfig.h"

#include "LogRecord.h"

namespace croniot::log {

// True only where RTC memory (fast or slow) actually exists on this
// target and therefore actually survives a reset. Confirmed real for the
// ESP32-C6 target this project ships on (CONFIG_SOC_RTC_FAST_MEM_SUPPORTED
// is set there). On a hypothetical target with neither, ESP-IDF's own
// RTC_NOINIT_ATTR macro silently degrades to nothing - the variable would
// still compile, but as an ordinary non-persistent global, which would
// make this class lie about surviving resets. Rather than let that happen
// silently, the persistence-bearing members below are compiled out
// entirely on such a target and replaced with no-ops (see .cpp); check
// this constant if you need to know which mode is active.
inline constexpr bool kRtcCriticalStoreIsPersistent =
#if CONFIG_SOC_RTC_FAST_MEM_SUPPORTED || CONFIG_SOC_RTC_SLOW_MEM_SUPPORTED
    true;
#else
    false;
#endif

// Same trivial-struct discipline as NoinitRingHeader (see NoinitRing.h) -
// no default member initializers, no methods - for the same reason: this
// header lives in RTC memory (RTC_NOINIT_ATTR), and a non-trivial default
// constructor would get re-run at every boot regardless of section
// placement, wiping the data this exists to preserve.
struct RtcCriticalStoreHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;      // CRC32 over {magic, version, count} only - same
                          // "don't touch the payload every boot" reasoning
                          // as NoinitRing's header CRC.
    uint32_t count;       // number of valid entries in records[], <= capacity
};

static_assert(std::is_trivial_v<RtcCriticalStoreHeader>,
              "RtcCriticalStoreHeader must stay trivial for RTC_NOINIT_ATTR placement");

// Tiny, fixed-capacity store of the highest-value records only: Level::Error
// lines and croniot::log::event() calls (see Log.h) - NOT every record,
// this is the ~1-2KB RTC budget from the plan, not a second copy of the
// RAM ring. Write-only from the hot path (push, append-and-drop-oldest);
// reading back (for a future crash-summary feature) is PR10's job - PR7
// only needs push() to exist and to actually persist across a reset.
class RtcCriticalStore {
public:
    // Same validate-or-reset contract as NoinitRing::init(): if the header
    // doesn't check out, this is a normal empty start, not an incident.
    static void init();

    static void push(const LogRecord& record);

    static uint32_t count();

    // Reports 0 on a target where RTC memory doesn't actually exist
    // (see kRtcCriticalStoreIsPersistent above) - a nonzero capacity would
    // wrongly imply this store keeps anything across a reset there.
    static constexpr uint32_t capacity() { return kRtcCriticalStoreIsPersistent ? kCapacity : 0u; }

private:
    static constexpr uint32_t kCapacity =
#ifdef CONFIG_CRONIOT_LOG_RTC_RECORDS
        CONFIG_CRONIOT_LOG_RTC_RECORDS;
#else
        6;
#endif
};

}  // namespace croniot::log

#endif
