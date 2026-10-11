#include "log/WriteAccounting.h"

#include <gtest/gtest.h>

using croniot::log::Stream;
using croniot::log::WriteAccounting;

TEST(WriteAccountingTest, LifetimeBytesAccumulateAcrossDays) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 100, /*dayIndex=*/1);
    acc.addBytesWritten(Stream::Logs, 50, /*dayIndex=*/2);
    EXPECT_EQ(acc.lifetimeBytes(Stream::Logs), 150u);
}

TEST(WriteAccountingTest, StreamsAreIndependent) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 100, 1);
    acc.addBytesWritten(Stream::Data, 5, 1);
    EXPECT_EQ(acc.lifetimeBytes(Stream::Logs), 100u);
    EXPECT_EQ(acc.lifetimeBytes(Stream::Data), 5u);
    EXPECT_EQ(acc.lifetimeBytes(Stream::Events), 0u);
}

TEST(WriteAccountingTest, BytesTodayResetsOnNewDayIndex) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 100, /*dayIndex=*/1);
    acc.addBytesWritten(Stream::Logs, 30, /*dayIndex=*/1);
    EXPECT_EQ(acc.bytesToday(Stream::Logs, 1), 130u);

    acc.addBytesWritten(Stream::Logs, 10, /*dayIndex=*/2);
    EXPECT_EQ(acc.bytesToday(Stream::Logs, 2), 10u);
    EXPECT_EQ(acc.bytesToday(Stream::Logs, 1), 0u);  // no longer "today"
}

TEST(WriteAccountingTest, DailyBudgetExceeded) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 900, 1);
    EXPECT_FALSE(acc.dailyBudgetExceeded(Stream::Logs, 1, /*maxBytesPerDay=*/1000));
    acc.addBytesWritten(Stream::Logs, 200, 1);
    EXPECT_TRUE(acc.dailyBudgetExceeded(Stream::Logs, 1, 1000));
}

TEST(WriteAccountingTest, ZeroBudgetMeansNoBudget) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 1'000'000, 1);
    EXPECT_FALSE(acc.dailyBudgetExceeded(Stream::Logs, 1, /*maxBytesPerDay=*/0));
}

TEST(WriteAccountingTest, EstimatedWearPercent) {
    WriteAccounting acc;
    // 100,000 cycles endurance, 1 MB partition: writing 1 MB total is one
    // full-partition-equivalent rewrite, i.e. 1/100000 = 0.001% of life.
    double wear = acc.estimatedWearPercent(/*totalBytesOnPartition=*/1'000'000,
                                            /*partitionSizeBytes=*/1'000'000,
                                            /*enduranceCycles=*/100'000);
    EXPECT_NEAR(wear, 0.001, 1e-9);
}

TEST(WriteAccountingTest, SnapshotRoundTrips) {
    WriteAccounting acc;
    acc.addBytesWritten(Stream::Logs, 111, 1);
    acc.addBytesWritten(Stream::Events, 22, 1);
    acc.addBytesWritten(Stream::Data, 3, 1);

    WriteAccounting restored;
    restored.restore(acc.snapshot());
    EXPECT_EQ(restored.lifetimeBytes(Stream::Logs), 111u);
    EXPECT_EQ(restored.lifetimeBytes(Stream::Events), 22u);
    EXPECT_EQ(restored.lifetimeBytes(Stream::Data), 3u);
}
