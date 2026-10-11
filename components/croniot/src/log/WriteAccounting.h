#ifndef CRONIOT_LOG_WRITEACCOUNTING_H
#define CRONIOT_LOG_WRITEACCOUNTING_H

#include <array>
#include <cstdint>

#include "JournalTypes.h"

namespace croniot::log {

// Counts bytes written per stream (plan §3.10) - lifetime totals for
// wear estimation, plus a same-day running total for the daily budget.
// `dayIndex` is supplied by the caller (e.g. days-since-boot or an
// epoch day once SNTP exists) rather than read from a clock here, same
// determinism reasoning as LevelResolver's explicit `nowMs`: this class
// has no opinion about what a "day" is, only that a new index means a
// new day.
class WriteAccounting {
public:
    void addBytesWritten(Stream stream, uint32_t bytes, uint32_t dayIndex) {
        auto& counter = counters_[index(stream)];
        counter.lifetimeBytes += bytes;
        if (dayIndex != counter.currentDayIndex) {
            counter.currentDayIndex = dayIndex;
            counter.bytesToday = 0;
        }
        counter.bytesToday += bytes;
    }

    uint64_t lifetimeBytes(Stream stream) const { return counters_[index(stream)].lifetimeBytes; }

    // 0 if `dayIndex` isn't the day this stream last wrote on (i.e. it
    // wrote nothing today) - not an error, just the honest answer.
    uint32_t bytesToday(Stream stream, uint32_t dayIndex) const {
        const auto& counter = counters_[index(stream)];
        return counter.currentDayIndex == dayIndex ? counter.bytesToday : 0;
    }

    bool dailyBudgetExceeded(Stream stream, uint32_t dayIndex, uint32_t maxBytesPerDay) const {
        return maxBytesPerDay > 0 && bytesToday(stream, dayIndex) > maxBytesPerDay;
    }

    // Cheap, deliberately-conservative estimate (plan §3.10: "se cuenta
    // el payload, no la amplificación de escritura real - es una cota
    // inferior"): total bytes written to streams sharing one partition,
    // divided by that partition's size, gives "equivalent full-partition
    // rewrites"; dividing again by the sector endurance in cycles gives
    // a rough percentage of estimated life consumed.
    double estimatedWearPercent(uint64_t totalBytesOnPartition, uint64_t partitionSizeBytes,
                                 uint32_t enduranceCycles) const {
        if (partitionSizeBytes == 0 || enduranceCycles == 0) return 0.0;
        double cyclesEquivalent =
            static_cast<double>(totalBytesOnPartition) / static_cast<double>(partitionSizeBytes);
        return cyclesEquivalent / static_cast<double>(enduranceCycles) * 100.0;
    }

    struct Snapshot {
        uint64_t lifetimeLogs = 0;
        uint64_t lifetimeEvents = 0;
        uint64_t lifetimeData = 0;
    };

    // For periodic NVS consolidation (plan: "nunca en cada escritura") -
    // the daily running totals are intentionally NOT persisted: losing
    // "bytes written so far today" on an unclean reset just means the
    // budget re-arms a little early, which is the safe direction to err.
    Snapshot snapshot() const {
        return {counters_[index(Stream::Logs)].lifetimeBytes,
                counters_[index(Stream::Events)].lifetimeBytes,
                counters_[index(Stream::Data)].lifetimeBytes};
    }

    void restore(const Snapshot& snapshot) {
        counters_[index(Stream::Logs)].lifetimeBytes = snapshot.lifetimeLogs;
        counters_[index(Stream::Events)].lifetimeBytes = snapshot.lifetimeEvents;
        counters_[index(Stream::Data)].lifetimeBytes = snapshot.lifetimeData;
    }

private:
    struct Counter {
        uint64_t lifetimeBytes = 0;
        uint32_t currentDayIndex = 0;
        uint32_t bytesToday = 0;
    };

    static size_t index(Stream stream) { return static_cast<size_t>(stream); }

    std::array<Counter, 3> counters_{};
};

}  // namespace croniot::log

#endif
