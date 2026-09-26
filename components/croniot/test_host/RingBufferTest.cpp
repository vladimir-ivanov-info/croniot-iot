#include <gtest/gtest.h>

#include "log/RingBuffer.h"

using croniot::log::RingBuffer;

TEST(RingBuffer, EmptyBufferPopsNothing) {
    RingBuffer<int, 4> ring;
    EXPECT_TRUE(ring.empty());
    EXPECT_FALSE(ring.pop().has_value());
}

TEST(RingBuffer, PushThenPopPreservesOrder) {
    RingBuffer<int, 4> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);

    EXPECT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring.pop(), 1);
    EXPECT_EQ(ring.pop(), 2);
    EXPECT_EQ(ring.pop(), 3);
    EXPECT_TRUE(ring.empty());
}

TEST(RingBuffer, OverwritesOldestWhenFullAndCountsDrops) {
    RingBuffer<int, 3> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);
    ring.push(4);  // buffer full at 3: evicts the unread "1"

    EXPECT_EQ(ring.droppedCount(), 1u);
    EXPECT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring.pop(), 2);
    EXPECT_EQ(ring.pop(), 3);
    EXPECT_EQ(ring.pop(), 4);
}

TEST(RingBuffer, ContinuesWorkingAfterWraparound) {
    RingBuffer<int, 2> ring;
    for (int i = 0; i < 10; ++i) {
        ring.push(i);
    }
    // Only the last 2 survive; 8 were dropped.
    EXPECT_EQ(ring.droppedCount(), 8u);
    EXPECT_EQ(ring.pop(), 8);
    EXPECT_EQ(ring.pop(), 9);
    EXPECT_TRUE(ring.empty());
}

TEST(RingBuffer, PartialReadThenMoreWritesDoesNotDoubleDrop) {
    RingBuffer<int, 3> ring;
    ring.push(1);
    ring.push(2);
    EXPECT_EQ(ring.pop(), 1);  // consumer catches up partially

    ring.push(3);
    ring.push(4);  // buffer holds {2,3,4}, nothing evicted (only 2 unread before this push, capacity 3)

    EXPECT_EQ(ring.droppedCount(), 0u);
    EXPECT_EQ(ring.pop(), 2);
    EXPECT_EQ(ring.pop(), 3);
    EXPECT_EQ(ring.pop(), 4);
}
