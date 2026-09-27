#include "telemetry/UplinkScheduler.h"

#include <gtest/gtest.h>

using croniot::log::Stream;
using croniot::telemetry::StreamBacklog;
using croniot::telemetry::UplinkScheduler;

TEST(UplinkSchedulerTest, NothingToDoWhenAllEmpty) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, false, false, 0},
        {Stream::Logs, false, false, 0},
        {Stream::Data, false, false, 0},
    };
    EXPECT_FALSE(UplinkScheduler::pickNext(backlogs, 60000).has_value());
}

TEST(UplinkSchedulerTest, EventsBeatsLogsBeatsData) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, false, 100},
        {Stream::Data, true, false, 100},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, 60000), Stream::Events);
}

TEST(UplinkSchedulerTest, SkipsStreamAlreadyInFlight) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, /*inFlight=*/true, 100},
        {Stream::Logs, true, false, 100},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, 60000), Stream::Logs);
}

TEST(UplinkSchedulerTest, DataStarvationOverridesPriorityOnceAgedEnough) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Data, true, false, /*oldestPendingAgeMs=*/70000},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/60000), Stream::Data);
}

TEST(UplinkSchedulerTest, DataNotOldEnoughStillLosesToEvents) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Data, true, false, /*oldestPendingAgeMs=*/10000},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/60000), Stream::Events);
}

TEST(UplinkSchedulerTest, StarvedDataAlreadyInFlightDoesNotOverride) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Data, true, /*inFlight=*/true, /*oldestPendingAgeMs=*/999999},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, 60000), Stream::Events);
}

TEST(UplinkSchedulerTest, MissingStreamEntryTreatedAsEmpty) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Data, true, false, 100},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, 60000), Stream::Data);
}
