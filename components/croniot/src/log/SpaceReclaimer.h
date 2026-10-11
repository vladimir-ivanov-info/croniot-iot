#ifndef CRONIOT_LOG_SPACERECLAIMER_H
#define CRONIOT_LOG_SPACERECLAIMER_H

#include <cstdint>
#include <vector>

#include "JournalTypes.h"

namespace croniot::log {

// One rotated-and-closed segment file, as seen by the reclaimer. Deletion
// unit is the whole segment (plan §8.8: "la unidad de borrado: el
// segmento o fichero rotado completo"), never a single record - so this
// is all the reclaimer needs to know about one.
struct SegmentInfo {
    Stream stream;
    uint32_t segmentId;  // monotonically increasing per stream at creation time; lower = older
    uint32_t oldestSeq;
    uint32_t newestSeq;  // inclusive
    uint32_t sizeBytes;
    bool fullyAcked;     // newestSeq <= that stream's ackedSeq
    uint64_t ageSeconds;  // time since the segment was closed/rotated
};

// Decides which closed segments to delete when free space drops below
// `lowWaterMarkPercent`, in the sacrifice order from the plan (§8.8):
//   1. anything fully acked by the server, oldest first;
//   2. raw `Data` older than the retention window, even if unacked;
//   3. `Logs` over quota, oldest first (approximated here as "still
//      short after 1-2: take the oldest Logs segments left");
//   4. only under `criticalMarkPercent`: breach the retention window
//      itself for `Data`.
// `Stream::Events` must never appear in `segments` - incidents and
// events are never sacrificed (plan: "nunca hasta el final") - this is
// enforced by the caller (Journal), which simply never lists them as
// candidates, rather than by a runtime check here.
class SpaceReclaimer {
public:
    struct Result {
        std::vector<uint32_t> segmentIdsToDelete;
        bool reachedLowWaterMark = false;
        bool hadToBreachRetentionWindow = false;
        bool degradedToRamOnly = false;  // even breaching retention wasn't enough
    };

    static Result reclaim(std::vector<SegmentInfo> segments, uint64_t totalBytes,
                           uint64_t freeBytes, uint64_t retentionSeconds,
                           double lowWaterMarkPercent = 20.0, double criticalMarkPercent = 5.0);
};

}  // namespace croniot::log

#endif
