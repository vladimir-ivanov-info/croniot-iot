#ifndef CRONIOT_SENSORS_REPORTPOLICY_H
#define CRONIOT_SENSORS_REPORTPOLICY_H

#include <cstdint>

namespace croniot {

// Plan §7.2: separates *sampling* (how often a sensor reads a value -
// the project's own concern, via samplePeriodMs) from *sending* (how
// those samples reach the server - this policy's concern).
//
// Only Immediate and Batch are implemented in this pass. Aggregate
// (send min/avg/max/count per window instead of every sample) and
// OnChange (send only when a value moves more than a deadband) are in
// the plan's table but deliberately not built here - there is no
// factory method for either below, on purpose, rather than accepting
// an enum value that would silently behave like Batch. A project that
// needs them today should keep polling at a coarser samplePeriodMs
// instead.
enum class ReportKind { Immediate, Batch };

struct ReportPolicy {
    ReportKind kind = ReportKind::Immediate;
    uint32_t periodSec = 0;   // Batch: max time a sample waits before a flush
    uint32_t maxSamples = 0;  // Batch: 0 = no count-based flush, only periodSec

    static ReportPolicy Immediate() { return ReportPolicy{ReportKind::Immediate, 0, 0}; }

    static ReportPolicy Batch(uint32_t periodSec, uint32_t maxSamples = 0) {
        return ReportPolicy{ReportKind::Batch, periodSec, maxSamples};
    }
};

}  // namespace croniot

#endif
