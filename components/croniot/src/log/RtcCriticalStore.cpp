#include "RtcCriticalStore.h"

#include "esp_attr.h"
#include "esp_crc.h"
#include "freertos/FreeRTOS.h"

namespace croniot::log {

#if CONFIG_SOC_RTC_FAST_MEM_SUPPORTED || CONFIG_SOC_RTC_SLOW_MEM_SUPPORTED

namespace {

constexpr uint32_t kMagic = 0x43524358;  // 'X','C','R','C' little-endian ascii, arbitrary but stable
constexpr uint32_t kVersion = 1;

// Trivial (see static_assert in the header) - RTC_NOINIT_ATTR keeps these
// across esp_restart()/panic/watchdog on a target with real RTC memory
// (confirmed for ESP32-C6, see kRtcCriticalStoreIsPersistent). Does NOT
// survive deep sleep or power loss even though RTC memory nominally can
// across *deep sleep* - RTC_NOINIT_ATTR specifically opts out of the
// deep-sleep-stub re-init IDF normally does for RTC_DATA_ATTR, but a full
// power loss still clears it regardless of attribute.
RTC_NOINIT_ATTR RtcCriticalStoreHeader g_header;
RTC_NOINIT_ATTR LogRecord g_records[RtcCriticalStore::capacity()];

portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

uint32_t headerCrc(const RtcCriticalStoreHeader& header) {
    uint32_t crc = 0;
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.magic), sizeof(header.magic));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.version), sizeof(header.version));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.count), sizeof(header.count));
    return crc;
}

void resetHeaderLocked() {
    g_header.magic = kMagic;
    g_header.version = kVersion;
    g_header.count = 0;
    g_header.crc32 = headerCrc(g_header);
}

}  // namespace

void RtcCriticalStore::init() {
    portENTER_CRITICAL(&g_lock);
    bool valid = g_header.magic == kMagic && g_header.version == kVersion &&
                 g_header.crc32 == headerCrc(g_header);
    if (!valid) {
        resetHeaderLocked();
    }
    portEXIT_CRITICAL(&g_lock);
}

void RtcCriticalStore::push(const LogRecord& record) {
    portENTER_CRITICAL(&g_lock);
    if (g_header.count < kCapacity) {
        g_records[g_header.count] = record;
        ++g_header.count;
    } else {
        // Full: drop the oldest entry to make room, same "never silently
        // lose the newest data" bias as NoinitRing, but the payload here
        // is tiny (Errors/events only) so a plain shift is cheap enough.
        for (uint32_t i = 1; i < kCapacity; ++i) {
            g_records[i - 1] = g_records[i];
        }
        g_records[kCapacity - 1] = record;
    }
    g_header.crc32 = headerCrc(g_header);
    portEXIT_CRITICAL(&g_lock);
}

uint32_t RtcCriticalStore::count() {
    portENTER_CRITICAL(&g_lock);
    uint32_t count = g_header.count;
    portEXIT_CRITICAL(&g_lock);
    return count;
}

#else  // no RTC memory on this target: no-op, deliberately - see
       // kRtcCriticalStoreIsPersistent in RtcCriticalStore.h. Nothing here
       // is declared with RTC_NOINIT_ATTR (it would just degrade to a
       // plain, non-persistent global on such a target - see esp_attr.h -
       // which would make this class silently lie about surviving resets).

void RtcCriticalStore::init() {}
void RtcCriticalStore::push(const LogRecord&) {}
uint32_t RtcCriticalStore::count() { return 0; }

#endif

}  // namespace croniot::log
