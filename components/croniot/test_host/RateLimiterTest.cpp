#include <gtest/gtest.h>

#include "log/RateLimiter.h"

using croniot::log::Level;
using croniot::log::LogRecord;
using croniot::log::RateLimiter;

namespace {
LogRecord makeRecord(Level level, const char* tag, const char* message) {
    LogRecord record{};
    record.level = level;
    record.setTag(tag);
    record.setMessage(message);
    return record;
}
}  // namespace

TEST(RateLimiter, DistinctRecordsAlwaysEmitWithNoRepeatFlag) {
    RateLimiter limiter;

    auto r1 = limiter.feed(makeRecord(Level::Info, "Tag", "first"));
    EXPECT_TRUE(r1.emitNow);
    EXPECT_FALSE(r1.pendingFlush.has_value());

    auto r2 = limiter.feed(makeRecord(Level::Info, "Tag", "second"));
    EXPECT_TRUE(r2.emitNow);
    EXPECT_FALSE(r2.pendingFlush.has_value());
}

TEST(RateLimiter, RepeatedIdenticalLinesAreSuppressedNotDropped) {
    RateLimiter limiter;
    LogRecord same = makeRecord(Level::Warn, "Wifi", "reconnecting");

    auto first = limiter.feed(same);
    EXPECT_TRUE(first.emitNow);

    auto second = limiter.feed(same);
    EXPECT_FALSE(second.emitNow);  // absorbed into the run, not lost

    auto third = limiter.feed(same);
    EXPECT_FALSE(third.emitNow);
}

TEST(RateLimiter, ADifferentRecordFlushesThePendingRepeatSummary) {
    RateLimiter limiter;
    LogRecord repeated = makeRecord(Level::Warn, "Wifi", "reconnecting");

    limiter.feed(repeated);  // 1st: emits
    limiter.feed(repeated);  // 2nd: suppressed
    limiter.feed(repeated);  // 3rd: suppressed

    auto changed = limiter.feed(makeRecord(Level::Info, "Wifi", "connected"));
    EXPECT_TRUE(changed.emitNow);
    ASSERT_TRUE(changed.pendingFlush.has_value());
    EXPECT_EQ(changed.pendingFlush->repeatCount, 2);  // two absorbed after the first
    EXPECT_STREQ(changed.pendingFlush->tag, "Wifi");
    EXPECT_STREQ(changed.pendingFlush->message, "reconnecting");
}

TEST(RateLimiter, FlushPendingReleasesAnOpenRunOnDemand) {
    RateLimiter limiter;
    LogRecord repeated = makeRecord(Level::Debug, "Tag", "same");

    limiter.feed(repeated);
    limiter.feed(repeated);

    auto flushed = limiter.flushPending();
    ASSERT_TRUE(flushed.has_value());
    EXPECT_EQ(flushed->repeatCount, 1);

    // Flushing clears the run; a second flush with nothing new has nothing to say.
    EXPECT_FALSE(limiter.flushPending().has_value());
}

TEST(RateLimiter, DifferentTagWithSameMessageIsNotConsideredARepeat) {
    RateLimiter limiter;
    limiter.feed(makeRecord(Level::Info, "TagA", "same text"));
    auto result = limiter.feed(makeRecord(Level::Info, "TagB", "same text"));
    EXPECT_TRUE(result.emitNow);
}
