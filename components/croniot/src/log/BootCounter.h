#ifndef CRONIOT_LOG_BOOTCOUNTER_H
#define CRONIOT_LOG_BOOTCOUNTER_H

#include <cstdint>

namespace croniot::log {

// The canonical `bootId` the plan's dedup key relies on everywhere
// (`(deviceUuid, bootId, stream, seq)` - §5): a small NVS counter,
// incremented once per boot. Read-and-increment happens the first time
// current() is called in a process's lifetime (Log::init() does this
// early); every later call in that same boot just returns the cached
// value without touching NVS again - this is a boot ordinal, not a
// per-record counter, so it doesn't need to be fast on the hot path,
// only correct exactly once per boot.
//
// Deliberately owned here rather than duplicated per-firmware: a
// firmware that already keeps its own separate boot counter for a
// human-readable log line (e.g. this SDK's own watering-system consumer)
// should read this value instead of incrementing a second NVS key -
// two independent counters under different keys never collide, but they
// also drift apart, and only this one is what the wire protocol and
// Journal-adjacent bootId actually mean.
class BootCounter {
public:
    static uint32_t current();
};

}  // namespace croniot::log

#endif
