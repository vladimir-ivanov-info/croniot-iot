#ifndef CRONIOT_LOG_NOINITRING_H
#define CRONIOT_LOG_NOINITRING_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include "LogRecord.h"

namespace croniot::log {

// Header for the crash-survivable RAM ring ("black box"): a handful of
// bytes placed in `.noinit` (see NoinitRing.cpp) that carries just enough
// bookkeeping to tell, at boot, whether the records[] array next to it is
// leftover data from a *previous* boot (esp_restart(), panic, watchdog -
// HP SRAM keeps its contents across all of those) or garbage (first boot
// ever, or the contents didn't survive - e.g. after a power loss/deep
// sleep, or if something else in the image now overlaps this address).
//
// Deliberately NO default member initializers and NO methods: exactly the
// same reasoning as LogRecord (see LogRecord.h) - a `.noinit`-placed global
// of a *non*-trivial type still runs its default constructor at every
// boot regardless of the NOLOAD section, silently wiping the black box
// before anyone gets to read it. static_assert below is the tripwire.
struct NoinitRingHeader {
    uint32_t magic;
    uint32_t version;
    // CRC32 over {magic, version, writeIndex, readIndex, dropped} only -
    // NOT over records[]. Recomputing a CRC over several KB of records on
    // every single push()/pop() (i.e. every log line) would be wasted work
    // on the hot path; the header fields are ~20 bytes, cheap enough to
    // touch on every call. This means the CRC can't detect corruption
    // *inside* an individual record - that's an accepted gap for PR7
    // (records are still bounds-checked via the index math either way).
    uint32_t crc32;
    uint32_t writeIndex;  // ever-increasing; storage slot is writeIndex % capacity
    uint32_t readIndex;
    uint32_t dropped;
};

static_assert(std::is_trivial_v<NoinitRingHeader>,
              "NoinitRingHeader must stay trivial for .noinit placement");

// Crash-survivable ring of LogRecord, backed by two `.noinit` globals (a
// header and a fixed-size array of CONFIG_CRONIOT_LOG_RAM_RECORDS
// records - see NoinitRing.cpp). Same eviction algorithm as RingBuffer.h
// (oldest unread record is overwritten, never a silent drop), but hand-
// rolled instead of instantiating RingBuffer<LogRecord, N> directly: that
// template has default member initializers on its own fields, so it is
// not std::is_trivial_v regardless of T, and would get its constructor
// re-run at boot in `.noinit` just like a naively-written LogRecord would.
//
// push() is called from any task via the esp_log hook, potentially
// concurrently; pop() is called only from the drain task, but concurrently
// with push() from other tasks. Both take the same internal spinlock -
// the critical section is index math plus one LogRecord copy, nothing
// that allocates or blocks.
class NoinitRing {
public:
    // Validates the header (magic + version + CRC over the header fields).
    // If it doesn't check out - first boot ever, corruption, or anything
    // that didn't preserve `.noinit` (power loss, deep sleep) - resets it
    // to empty. If it DOES check out, leaves writeIndex/readIndex/dropped
    // exactly as they were: whatever the previous boot hadn't drained yet
    // stays queued and the drain task consumes it exactly like new data.
    static void init();

    static void push(const LogRecord& record);
    static std::optional<LogRecord> pop();

    static uint32_t droppedCount();
    static uint32_t size();
    static constexpr uint32_t capacity() { return kCapacity; }

private:
    static constexpr uint32_t kCapacity =
#ifdef CONFIG_CRONIOT_LOG_RAM_RECORDS
        CONFIG_CRONIOT_LOG_RAM_RECORDS;
#else
        32;
#endif
};

}  // namespace croniot::log

#endif
