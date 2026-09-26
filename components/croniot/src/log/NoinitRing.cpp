#include "NoinitRing.h"

#include "esp_attr.h"
#include "esp_crc.h"
#include "freertos/FreeRTOS.h"

namespace croniot::log {

namespace {

constexpr uint32_t kMagic = 0x474F4C43;    // 'C','L','O','G' little-endian ascii, arbitrary but stable
constexpr uint32_t kVersion = 1;

// Both are trivial (see static_assert in NoinitRing.h / LogRecord.h), so
// the compiler emits no constructor call for either - the linker places
// them in `.noinit` (NOLOAD) and they keep whatever bit pattern was there
// across esp_restart()/panic/watchdog resets. They do NOT survive deep
// sleep or a power-on reset (HP SRAM isn't retained in either case) -
// init() below tells those cases apart from "real" survival via the
// magic/version/CRC check, not by asking esp_reset_reason().
__NOINIT_ATTR NoinitRingHeader g_header;
__NOINIT_ATTR LogRecord g_records[NoinitRing::capacity()];

portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

uint32_t headerCrc(const NoinitRingHeader& header) {
    uint32_t crc = 0;
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.magic), sizeof(header.magic));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.version), sizeof(header.version));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.writeIndex), sizeof(header.writeIndex));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.readIndex), sizeof(header.readIndex));
    crc = esp_crc32_le(crc, reinterpret_cast<const uint8_t*>(&header.dropped), sizeof(header.dropped));
    return crc;
}

void resetHeaderLocked() {
    g_header.magic = kMagic;
    g_header.version = kVersion;
    g_header.writeIndex = 0;
    g_header.readIndex = 0;
    g_header.dropped = 0;
    g_header.crc32 = headerCrc(g_header);
}

}  // namespace

void NoinitRing::init() {
    portENTER_CRITICAL(&g_lock);
    bool valid = g_header.magic == kMagic && g_header.version == kVersion &&
                 g_header.crc32 == headerCrc(g_header);
    if (!valid) {
        // First boot ever, corruption, or a reset flavour that didn't
        // preserve `.noinit` (power-on, brownout, deep sleep). Starting
        // empty here is the normal case, not an incident.
        resetHeaderLocked();
    }
    // If valid, deliberately leave writeIndex/readIndex/dropped untouched:
    // whatever the previous boot hadn't drained yet is still queued and
    // will be popped by the drain task exactly like new data.
    portEXIT_CRITICAL(&g_lock);
}

void NoinitRing::push(const LogRecord& record) {
    portENTER_CRITICAL(&g_lock);
    g_records[g_header.writeIndex % kCapacity] = record;
    ++g_header.writeIndex;
    if (g_header.writeIndex - g_header.readIndex > kCapacity) {
        // Producer lapped the consumer: the slot just overwritten held an
        // unread record. Advance readIndex past it and count the loss -
        // same "never drop in silence" contract as RingBuffer.h.
        g_header.readIndex = g_header.writeIndex - kCapacity;
        ++g_header.dropped;
    }
    g_header.crc32 = headerCrc(g_header);
    portEXIT_CRITICAL(&g_lock);
}

std::optional<LogRecord> NoinitRing::pop() {
    std::optional<LogRecord> result;
    portENTER_CRITICAL(&g_lock);
    if (g_header.readIndex != g_header.writeIndex) {
        result = g_records[g_header.readIndex % kCapacity];
        ++g_header.readIndex;
        g_header.crc32 = headerCrc(g_header);
    }
    portEXIT_CRITICAL(&g_lock);
    return result;
}

uint32_t NoinitRing::droppedCount() {
    portENTER_CRITICAL(&g_lock);
    uint32_t dropped = g_header.dropped;
    portEXIT_CRITICAL(&g_lock);
    return dropped;
}

uint32_t NoinitRing::size() {
    portENTER_CRITICAL(&g_lock);
    uint32_t size = g_header.writeIndex - g_header.readIndex;
    portEXIT_CRITICAL(&g_lock);
    return size;
}

}  // namespace croniot::log
