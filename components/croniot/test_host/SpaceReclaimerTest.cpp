#include "log/SpaceReclaimer.h"

#include <gtest/gtest.h>

using croniot::log::SegmentInfo;
using croniot::log::SpaceReclaimer;
using croniot::log::Stream;

namespace {

SegmentInfo makeSegment(Stream stream, uint32_t id, uint32_t sizeBytes, bool acked,
                         uint64_t ageSeconds) {
    return SegmentInfo{stream, id, id * 100, id * 100 + 99, sizeBytes, acked, ageSeconds};
}

}  // namespace

TEST(SpaceReclaimerTest, NothingToDoAboveLowWaterMark) {
    auto result = SpaceReclaimer::reclaim({}, /*totalBytes=*/1000, /*freeBytes=*/300, 3600);
    EXPECT_TRUE(result.reachedLowWaterMark);
    EXPECT_TRUE(result.segmentIdsToDelete.empty());
}

TEST(SpaceReclaimerTest, AckedSegmentsGoFirstOldestFirst) {
    std::vector<SegmentInfo> segments = {
        makeSegment(Stream::Logs, /*id=*/3, 100, /*acked=*/true, 10),
        makeSegment(Stream::Logs, /*id=*/1, 100, /*acked=*/true, 10),
        makeSegment(Stream::Logs, /*id=*/2, 100, /*acked=*/true, 10),
    };
    // 5% free of 1000 = 50 bytes; target for 20% low water mark = 200 bytes.
    auto result = SpaceReclaimer::reclaim(segments, 1000, 50, 3600, /*lowWaterMarkPercent=*/20.0);
    ASSERT_EQ(result.segmentIdsToDelete.size(), 2u);
    EXPECT_EQ(result.segmentIdsToDelete[0], 1u);
    EXPECT_EQ(result.segmentIdsToDelete[1], 2u);
    EXPECT_TRUE(result.reachedLowWaterMark);
    EXPECT_FALSE(result.hadToBreachRetentionWindow);
}

TEST(SpaceReclaimerTest, DataOlderThanRetentionComesBeforeLogs) {
    std::vector<SegmentInfo> segments = {
        makeSegment(Stream::Logs, /*id=*/1, 200, /*acked=*/false, 10),
        makeSegment(Stream::Data, /*id=*/2, 200, /*acked=*/false, /*ageSeconds=*/999999),
    };
    auto result = SpaceReclaimer::reclaim(segments, 1000, 50, /*retentionSeconds=*/3600, 20.0);
    ASSERT_FALSE(result.segmentIdsToDelete.empty());
    EXPECT_EQ(result.segmentIdsToDelete[0], 2u);  // old, unacked Data before Logs
}

TEST(SpaceReclaimerTest, NeverTouchesEventsSegmentsBecauseCallerNeverListsThem) {
    // Events must simply never appear in `segments` - Journal enforces
    // that by construction. Confirms a Logs-only candidate list still
    // reclaims fine without needing an Events entry to reason about.
    std::vector<SegmentInfo> segments = {
        makeSegment(Stream::Logs, 1, 200, /*acked=*/false, 10),
    };
    auto result = SpaceReclaimer::reclaim(segments, 1000, 50, 3600, 20.0);
    EXPECT_EQ(result.segmentIdsToDelete.size(), 1u);
}

TEST(SpaceReclaimerTest, WithinRetentionDataOnlyReclaimedWhenCritical) {
    std::vector<SegmentInfo> segments = {
        makeSegment(Stream::Data, 1, 500, /*acked=*/false, /*ageSeconds=*/10),  // well within retention
    };
    // 10% free, low water mark 20%, critical mark 5%: not critical yet.
    auto result = SpaceReclaimer::reclaim(segments, 1000, 100, 3600, /*lowWaterMarkPercent=*/20.0,
                                           /*criticalMarkPercent=*/5.0);
    EXPECT_TRUE(result.segmentIdsToDelete.empty());
    EXPECT_TRUE(result.degradedToRamOnly);
    EXPECT_FALSE(result.hadToBreachRetentionWindow);
}

TEST(SpaceReclaimerTest, BreachesRetentionWindowOnlyUnderCriticalMark) {
    std::vector<SegmentInfo> segments = {
        makeSegment(Stream::Data, 1, 500, /*acked=*/false, /*ageSeconds=*/10),
    };
    // 3% free: below the 5% critical mark.
    auto result = SpaceReclaimer::reclaim(segments, 1000, 30, 3600, 20.0, 5.0);
    ASSERT_EQ(result.segmentIdsToDelete.size(), 1u);
    EXPECT_TRUE(result.hadToBreachRetentionWindow);
}

TEST(SpaceReclaimerTest, DegradesToRamOnlyWhenNothingLeftToSacrifice) {
    auto result = SpaceReclaimer::reclaim({}, 1000, 10, 3600, 20.0, 5.0);
    EXPECT_TRUE(result.segmentIdsToDelete.empty());
    EXPECT_TRUE(result.degradedToRamOnly);
}
