#include <gtest/gtest.h>

#include <vector>

#include "log/Span.h"

using croniot::log::resetSpanHooksForTesting;
using croniot::log::setSpanClock;
using croniot::log::setSpanSink;
using croniot::log::Span;

class SpanTest : public ::testing::Test {
protected:
    void SetUp() override { resetSpanHooksForTesting(); }
    void TearDown() override { resetSpanHooksForTesting(); }
};

TEST_F(SpanTest, ReportsElapsedDurationOnDestruction) {
    uint64_t fakeNow = 1000;
    setSpanClock([&fakeNow] { return fakeNow; });

    std::vector<std::pair<std::string, uint64_t>> calls;
    setSpanSink([&calls](const std::string& name, uint64_t durationUs) { calls.emplace_back(name, durationUs); });

    {
        Span span("mqtt.publish");
        fakeNow += 250;
    }  // span destroyed here

    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].first, "mqtt.publish");
    EXPECT_EQ(calls[0].second, 250u);
}

TEST_F(SpanTest, NestedSpansEachReportTheirOwnDuration) {
    uint64_t fakeNow = 0;
    setSpanClock([&fakeNow] { return fakeNow; });

    std::vector<std::pair<std::string, uint64_t>> calls;
    setSpanSink([&calls](const std::string& name, uint64_t durationUs) { calls.emplace_back(name, durationUs); });

    {
        Span outer("task.water");
        fakeNow += 10;
        {
            Span inner("valve.open");
            fakeNow += 5;
        }  // inner ends first
        fakeNow += 3;
    }  // outer ends

    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0].first, "valve.open");
    EXPECT_EQ(calls[0].second, 5u);
    EXPECT_EQ(calls[1].first, "task.water");
    EXPECT_EQ(calls[1].second, 18u);
}

TEST_F(SpanTest, IsSafeToUseWithNoHooksConfigured) {
    // No setSpanClock/setSpanSink call: must not crash, defaults are no-ops.
    Span span("unconfigured");
    (void)span;
}

TEST_F(SpanTest, MacroProducesAUniquelyNamedVariablePerLine) {
    uint64_t fakeNow = 0;
    setSpanClock([&fakeNow] { return fakeNow; });
    int callCount = 0;
    setSpanSink([&callCount](const std::string&, uint64_t) { ++callCount; });

    { CRONIOT_SPAN("a"); }
    { CRONIOT_SPAN("b"); }

    EXPECT_EQ(callCount, 2);
}
