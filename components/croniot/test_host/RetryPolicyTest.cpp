#include "telemetry/RetryPolicy.h"

#include <gtest/gtest.h>

using croniot::telemetry::RetryPolicy;

TEST(RetryPolicyTest, FirstAttemptUsesBaseDelay) {
    EXPECT_EQ(RetryPolicy::nextDelayMs(1, 30000, 300000, 0.0), 30000u);
}

TEST(RetryPolicyTest, DelayDoublesPerAttempt) {
    EXPECT_EQ(RetryPolicy::nextDelayMs(1, 1000, 1'000'000, 0.0), 1000u);
    EXPECT_EQ(RetryPolicy::nextDelayMs(2, 1000, 1'000'000, 0.0), 2000u);
    EXPECT_EQ(RetryPolicy::nextDelayMs(3, 1000, 1'000'000, 0.0), 4000u);
    EXPECT_EQ(RetryPolicy::nextDelayMs(4, 1000, 1'000'000, 0.0), 8000u);
}

TEST(RetryPolicyTest, DelayNeverExceedsCap) {
    EXPECT_EQ(RetryPolicy::nextDelayMs(10, 1000, 5000, 0.0), 5000u);
    EXPECT_EQ(RetryPolicy::nextDelayMs(50, 1000, 5000, 0.0), 5000u);
}

TEST(RetryPolicyTest, JitterOnlyAddsNeverSubtracts) {
    uint64_t base = RetryPolicy::nextDelayMs(2, 1000, 1'000'000, 0.0);
    uint64_t jittered = RetryPolicy::nextDelayMs(2, 1000, 1'000'000, 0.5);
    EXPECT_GE(jittered, base);
    EXPECT_EQ(jittered, base + base / 2);
}

TEST(RetryPolicyTest, JitterFractionIsClamped) {
    uint64_t delay = RetryPolicy::nextDelayMs(1, 1000, 1'000'000, 5.0);  // out of range
    EXPECT_EQ(delay, 2000u);  // clamped to 1.0 -> +100%
}

TEST(RetryPolicyTest, GivesUpAtMaxAttempts) {
    EXPECT_FALSE(RetryPolicy::shouldGiveUp(4, 5));
    EXPECT_TRUE(RetryPolicy::shouldGiveUp(5, 5));
    EXPECT_TRUE(RetryPolicy::shouldGiveUp(6, 5));
}
