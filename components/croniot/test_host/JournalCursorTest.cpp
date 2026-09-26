#include "log/JournalCursor.h"

#include <gtest/gtest.h>

using croniot::log::JournalCursor;
using croniot::log::Stream;

TEST(JournalCursorTest, AssignSeqStartsAtZeroAndIncrements) {
    JournalCursor cursor(Stream::Logs);
    EXPECT_EQ(cursor.assignSeq(), 0u);
    EXPECT_EQ(cursor.assignSeq(), 1u);
    EXPECT_EQ(cursor.assignSeq(), 2u);
    EXPECT_EQ(cursor.nextSeq(), 3u);
}

TEST(JournalCursorTest, NoAckInitially) {
    JournalCursor cursor(Stream::Logs);
    EXPECT_FALSE(cursor.hasAck());
}

TEST(JournalCursorTest, AckNeverMovesBackwards) {
    JournalCursor cursor(Stream::Data);
    cursor.ack(10);
    EXPECT_EQ(cursor.ackedSeq(), 10u);
    cursor.ack(5);  // stale/duplicate ack from a reconnect
    EXPECT_EQ(cursor.ackedSeq(), 10u);
    cursor.ack(20);
    EXPECT_EQ(cursor.ackedSeq(), 20u);
}

TEST(JournalCursorTest, ReclaimOfFullyAckedRangeProducesNoGap) {
    JournalCursor cursor(Stream::Logs);
    for (int i = 0; i < 100; ++i) cursor.assignSeq();
    cursor.ack(59);
    auto gap = cursor.reclaimTo(60, "space_pressure");
    EXPECT_FALSE(gap.has_value());
    EXPECT_EQ(cursor.oldestSeq(), 60u);
}

TEST(JournalCursorTest, ReclaimOfUnackedRangeProducesGap) {
    JournalCursor cursor(Stream::Data);
    for (int i = 0; i < 100; ++i) cursor.assignSeq();
    cursor.ack(10);
    auto gap = cursor.reclaimTo(30, "space_pressure");
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->stream, Stream::Data);
    EXPECT_EQ(gap->fromSeq, 11u);
    EXPECT_EQ(gap->toSeq, 30u);
    EXPECT_EQ(gap->count, 19u);
}

TEST(JournalCursorTest, ReclaimWithNoAckAtAllTreatsEverythingAsUnacked) {
    JournalCursor cursor(Stream::Logs);
    for (int i = 0; i < 50; ++i) cursor.assignSeq();
    auto gap = cursor.reclaimTo(20, "no_server_ever");
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->fromSeq, 0u);
    EXPECT_EQ(gap->toSeq, 20u);
    EXPECT_EQ(gap->count, 20u);
}

TEST(JournalCursorTest, ReclaimNotAdvancingIsANoOp) {
    JournalCursor cursor(Stream::Logs);
    for (int i = 0; i < 50; ++i) cursor.assignSeq();
    cursor.reclaimTo(20, "first");
    auto gap = cursor.reclaimTo(20, "again");  // same boundary, nothing new
    EXPECT_FALSE(gap.has_value());
    EXPECT_EQ(cursor.oldestSeq(), 20u);
}

TEST(JournalCursorTest, RestoreSetsAllFieldsExactly) {
    JournalCursor cursor(Stream::Events);
    cursor.restore(/*nextSeq=*/42, /*oldestSeq=*/7, /*ackedSeq=*/std::optional<uint32_t>(30));
    EXPECT_EQ(cursor.nextSeq(), 42u);
    EXPECT_EQ(cursor.oldestSeq(), 7u);
    EXPECT_TRUE(cursor.hasAck());
    EXPECT_EQ(cursor.ackedSeq(), 30u);
}

TEST(JournalCursorTest, RestoreWithNoAckLeavesHasAckFalse) {
    JournalCursor cursor(Stream::Events);
    cursor.restore(10, 0, std::nullopt);
    EXPECT_FALSE(cursor.hasAck());
}
