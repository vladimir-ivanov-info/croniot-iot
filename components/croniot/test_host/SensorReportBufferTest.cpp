#include "Sensors/SensorReportBuffer.h"

#include <gtest/gtest.h>

using croniot::ReportPolicy;
using croniot::SensorReportBuffer;
using croniot::SensorSample;

TEST(SensorReportBufferTest, ImmediateSignalsFlushOnEveryAdd) {
    SensorReportBuffer buffer(ReportPolicy::Immediate());
    EXPECT_TRUE(buffer.add({1000, 21.5}));
    EXPECT_TRUE(buffer.add({2000, 21.6}));
}

TEST(SensorReportBufferTest, ImmediateShouldFlushIsAlwaysFalse) {
    SensorReportBuffer buffer(ReportPolicy::Immediate());
    buffer.add({1000, 21.5});
    EXPECT_FALSE(buffer.shouldFlush(999999));
}

TEST(SensorReportBufferTest, BatchDoesNotSignalFlushFromAdd) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/60));
    EXPECT_FALSE(buffer.add({1000, 21.5}));
    EXPECT_FALSE(buffer.add({2000, 21.6}));
}

TEST(SensorReportBufferTest, BatchFlushesOnceThePeriodElapsesSinceTheOldestSample) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/60));
    buffer.add({1000, 21.5});
    EXPECT_FALSE(buffer.shouldFlush(1000 + 59000));
    EXPECT_TRUE(buffer.shouldFlush(1000 + 60000));
}

TEST(SensorReportBufferTest, BatchFlushesOnceMaxSamplesReachedRegardlessOfTime) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/3600, /*maxSamples=*/3));
    buffer.add({1000, 1.0});
    buffer.add({2000, 2.0});
    EXPECT_FALSE(buffer.shouldFlush(2001));
    buffer.add({3000, 3.0});
    EXPECT_TRUE(buffer.shouldFlush(3001));
}

TEST(SensorReportBufferTest, EmptyBufferNeverShouldFlush) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/1));
    EXPECT_FALSE(buffer.shouldFlush(999999999));
}

TEST(SensorReportBufferTest, FlushReturnsEverythingBufferedAndClears) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/60));
    buffer.add({1000, 1.0});
    buffer.add({2000, 2.0});

    auto samples = buffer.flush();
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].value, 1.0);
    EXPECT_EQ(samples[1].value, 2.0);
    EXPECT_FALSE(buffer.hasPending());
    EXPECT_FALSE(buffer.shouldFlush(999999999));
}

TEST(SensorReportBufferTest, FlushAfterFlushStartsAFreshWindow) {
    SensorReportBuffer buffer(ReportPolicy::Batch(/*periodSec=*/60));
    buffer.add({1000, 1.0});
    buffer.flush();

    buffer.add({100000, 2.0});
    EXPECT_FALSE(buffer.shouldFlush(100000 + 59000));
    EXPECT_TRUE(buffer.shouldFlush(100000 + 60000));
}
