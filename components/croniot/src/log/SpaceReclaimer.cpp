#include "SpaceReclaimer.h"

#include <algorithm>

namespace croniot::log {

SpaceReclaimer::Result SpaceReclaimer::reclaim(std::vector<SegmentInfo> segments,
                                                uint64_t totalBytes, uint64_t freeBytes,
                                                uint64_t retentionSeconds,
                                                double lowWaterMarkPercent,
                                                double criticalMarkPercent) {
    Result result;
    if (totalBytes == 0) return result;

    double percentFree = static_cast<double>(freeBytes) * 100.0 / static_cast<double>(totalBytes);
    if (percentFree >= lowWaterMarkPercent) {
        result.reachedLowWaterMark = true;
        return result;
    }

    uint64_t targetFreeBytes =
        static_cast<uint64_t>(static_cast<double>(totalBytes) * lowWaterMarkPercent / 100.0);
    bool critical = percentFree < criticalMarkPercent;

    auto byAge = [](const SegmentInfo& a, const SegmentInfo& b) { return a.segmentId < b.segmentId; };

    std::vector<SegmentInfo> tier1, tier2, tier3, tier4;
    for (const auto& segment : segments) {
        if (segment.fullyAcked) {
            tier1.push_back(segment);
        } else if (segment.stream == Stream::Data && segment.ageSeconds > retentionSeconds) {
            tier2.push_back(segment);
        } else if (segment.stream == Stream::Logs) {
            tier3.push_back(segment);
        } else if (segment.stream == Stream::Data) {
            tier4.push_back(segment);  // still within retention window - last resort
        }
    }
    std::sort(tier1.begin(), tier1.end(), byAge);
    std::sort(tier2.begin(), tier2.end(), byAge);
    std::sort(tier3.begin(), tier3.end(), byAge);
    std::sort(tier4.begin(), tier4.end(), byAge);

    uint64_t freed = freeBytes;
    auto consume = [&](const std::vector<SegmentInfo>& tier) {
        for (const auto& segment : tier) {
            if (freed >= targetFreeBytes) return;
            result.segmentIdsToDelete.push_back(segment.segmentId);
            freed += segment.sizeBytes;
        }
    };

    consume(tier1);
    consume(tier2);
    consume(tier3);
    if (freed < targetFreeBytes && critical) {
        result.hadToBreachRetentionWindow = !tier4.empty();
        consume(tier4);
    }

    result.reachedLowWaterMark = freed >= targetFreeBytes;
    result.degradedToRamOnly = !result.reachedLowWaterMark;
    return result;
}

}  // namespace croniot::log
