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

// A continuously-refilled Events stream never goes `inFlight` long - each
// batch's own `inFlight` window is where Logs would normally sneak in,
// but if Events is never empty, plain priority order alone starves Logs
// forever. The `logsStarvationMs` quota is what prevents that.
TEST(UplinkSchedulerTest, LogsStarvationOverridesEventsOnceAgedEnough) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, false, /*oldestPendingAgeMs=*/70000},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/999999999,
                                         /*logsStarvationMs=*/60000),
              Stream::Logs);
}

TEST(UplinkSchedulerTest, LogsNotOldEnoughStillLosesToEvents) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, false, /*oldestPendingAgeMs=*/10000},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/999999999,
                                         /*logsStarvationMs=*/60000),
              Stream::Events);
}

TEST(UplinkSchedulerTest, StarvedLogsAlreadyInFlightDoesNotOverride) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, /*inFlight=*/true, /*oldestPendingAgeMs=*/999999},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/999999999,
                                         /*logsStarvationMs=*/60000),
              Stream::Events);
}

// Data's own starvation check still wins over Logs's when both are aged
// enough and Data comes first in pickNext()'s checks - Data starving is
// strictly worse (it's the stream with the longest default patience), so
// this ordering is deliberate, not an accident of check order.
TEST(UplinkSchedulerTest, DataStarvationCheckedBeforeLogsStarvation) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, false, /*oldestPendingAgeMs=*/70000},
        {Stream::Data, true, false, /*oldestPendingAgeMs=*/70000},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, /*dataStarvationMs=*/60000,
                                         /*logsStarvationMs=*/60000),
              Stream::Data);
}

// Callers that don't pass logsStarvationMs (the default parameter) keep
// the pre-existing behavior exactly - no surprise fairness kicking in
// for code that hasn't opted in.
TEST(UplinkSchedulerTest, DefaultLogsStarvationNeverOverrides) {
    std::vector<StreamBacklog> backlogs = {
        {Stream::Events, true, false, 100},
        {Stream::Logs, true, false, /*oldestPendingAgeMs=*/999999999},
    };
    EXPECT_EQ(UplinkScheduler::pickNext(backlogs, 999999999), Stream::Events);
}
