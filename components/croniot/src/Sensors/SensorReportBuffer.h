#ifndef CRONIOT_SENSORS_SENSORREPORTBUFFER_H
#define CRONIOT_SENSORS_SENSORREPORTBUFFER_H

#include <cstdint>
#include <utility>
#include <vector>

#include "ReportPolicy.h"

namespace croniot {

struct SensorSample {
    uint64_t timestampMs;
    double value;
};

// Buffers samples for one sensor according to its ReportPolicy (plan
// §7.2). Pure logic: every "now" is passed in explicitly rather than
// read from a clock - same determinism pattern as LevelResolver/
// JournalCursor elsewhere in this SDK; the platform layer supplies real
// uptime.
class SensorReportBuffer {
public:
    explicit SensorReportBuffer(ReportPolicy policy) : policy_(policy) {}

    // Records one sample. Returns true if the caller should flush this
    // buffer *immediately* as a direct result of this sample -
    // ReportKind::Immediate only. Batch never returns true here; use
    // shouldFlush() to poll it instead.
    bool add(const SensorSample& sample) {
        pending_.push_back(sample);
        return policy_.kind == ReportKind::Immediate;
    }

    // Time/count-based flush check, meaningful for Batch only -
    // Immediate already signaled its own flush from add() and this
    // always returns false for it, even with samples pending (there
    // never should be any: a caller that flushes on add()'s true would
    // never leave one behind, but a caller that doesn't isn't this
    // class's problem to solve).
    bool shouldFlush(uint64_t nowMs) const {
        if (pending_.empty() || policy_.kind == ReportKind::Immediate) return false;
        if (policy_.maxSamples > 0 && pending_.size() >= policy_.maxSamples) return true;
        uint64_t oldestMs = pending_.front().timestampMs;
        return nowMs >= oldestMs + static_cast<uint64_t>(policy_.periodSec) * 1000;
    }

    bool hasPending() const { return !pending_.empty(); }

    // Consumes and returns everything currently buffered, clearing
    // state regardless of policy.
    std::vector<SensorSample> flush() {
        std::vector<SensorSample> result = std::move(pending_);
        pending_.clear();
        return result;
    }

private:
    ReportPolicy policy_;
    std::vector<SensorSample> pending_;
};

}  // namespace croniot

#endif
