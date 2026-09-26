#include <gtest/gtest.h>

#include "log/Counters.h"

using croniot::log::Counters;

class CountersTest : public ::testing::Test {
protected:
    void SetUp() override { Counters::instance().resetForTesting(); }
};

TEST_F(CountersTest, UnknownCounterStartsAtZero) {
    EXPECT_EQ(Counters::instance().get("never_incremented"), 0u);
}

TEST_F(CountersTest, IncrementDefaultsToOne) {
    Counters::instance().increment("mqtt.reconnect");
    EXPECT_EQ(Counters::instance().get("mqtt.reconnect"), 1u);
}

TEST_F(CountersTest, IncrementAccumulatesAndAcceptsACustomStep) {
    Counters::instance().increment("queue_full");
    Counters::instance().increment("queue_full", 5);
    EXPECT_EQ(Counters::instance().get("queue_full"), 6u);
}

TEST_F(CountersTest, SnapshotReturnsEverythingIncrementedSoFar) {
    Counters::instance().increment("a");
    Counters::instance().increment("b", 3);
    auto snapshot = Counters::instance().snapshot();
    EXPECT_EQ(snapshot["a"], 1u);
    EXPECT_EQ(snapshot["b"], 3u);
}

TEST_F(CountersTest, MacroIncrementsByName) {
    CRONIOT_COUNT(publish_failed);
    CRONIOT_COUNT(publish_failed);
    EXPECT_EQ(Counters::instance().get("publish_failed"), 2u);
}
