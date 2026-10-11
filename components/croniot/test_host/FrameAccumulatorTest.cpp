#include "log/FrameAccumulator.h"

#include <gtest/gtest.h>
#include <vector>

using croniot::log::accumulateFrames;
using croniot::log::AccumulateResult;

namespace {

void appendFrame(std::vector<uint8_t>& buf, const std::vector<uint8_t>& payload) {
    uint16_t len = static_cast<uint16_t>(payload.size());
    buf.push_back(static_cast<uint8_t>(len & 0xFF));
    buf.push_back(static_cast<uint8_t>(len >> 8));
    buf.insert(buf.end(), payload.begin(), payload.end());
}

struct Accumulator {
    uint32_t seq = 0;
    bool started = false;
    uint32_t firstSeq = 0;
    std::vector<uint8_t> frames;
    uint32_t count = 0;

    AccumulateResult run(const std::vector<uint8_t>& buf, uint32_t nextWantedSeq, size_t maxBytes,
                          size_t maxRecords) {
        return accumulateFrames(buf.data(), buf.size(), seq, nextWantedSeq, started, firstSeq, frames, count,
                                 maxBytes, maxRecords);
    }
};

}  // namespace

TEST(FrameAccumulatorTest, ConsumesAllCompleteFramesWhenUnderLimits) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2});
    appendFrame(buf, {3, 4, 5});

    Accumulator acc;
    auto result = acc.run(buf, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/1000);

    EXPECT_FALSE(result.limitReached);
    EXPECT_EQ(result.consumedBytes, buf.size());
    EXPECT_EQ(acc.count, 2u);
    EXPECT_EQ(acc.frames, (std::vector<uint8_t>{1, 2, 3, 4, 5}));
    EXPECT_TRUE(acc.started);
    EXPECT_EQ(acc.firstSeq, 0u);
    EXPECT_EQ(acc.seq, 2u);
}

TEST(FrameAccumulatorTest, SkipsFramesBeforeNextWantedSeq) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1});  // seq 0 - already consumed by an earlier batch
    appendFrame(buf, {2});  // seq 1 - already consumed
    appendFrame(buf, {3});  // seq 2 - wanted

    Accumulator acc;
    auto result = acc.run(buf, /*nextWantedSeq=*/2, /*maxBytes=*/1000, /*maxRecords=*/1000);

    EXPECT_FALSE(result.limitReached);
    EXPECT_EQ(acc.count, 1u);
    EXPECT_EQ(acc.frames, (std::vector<uint8_t>{3}));
    EXPECT_EQ(acc.firstSeq, 2u);
}

TEST(FrameAccumulatorTest, StopsAtMaxRecordsWithoutConsumingTheOverflowFrame) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1});
    appendFrame(buf, {2});
    appendFrame(buf, {3});  // should never be consumed - limit is 2 records

    Accumulator acc;
    auto result = acc.run(buf, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/2);

    EXPECT_TRUE(result.limitReached);
    EXPECT_EQ(acc.count, 2u);
    EXPECT_EQ(acc.frames, (std::vector<uint8_t>{1, 2}));
    // consumedBytes stops right after the 2nd frame, not including the 3rd.
    EXPECT_EQ(result.consumedBytes, 2 * 3u);  // 2 frames * (2-byte prefix + 1-byte payload)
}

TEST(FrameAccumulatorTest, StopsAtMaxBytesWithoutConsumingTheOverflowFrame) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2, 3});  // 3 bytes of payload
    appendFrame(buf, {4, 5, 6});  // pushes total payload to 6 - should never be consumed if maxBytes=3

    Accumulator acc;
    auto result = acc.run(buf, /*nextWantedSeq=*/0, /*maxBytes=*/3, /*maxRecords=*/1000);

    EXPECT_TRUE(result.limitReached);
    EXPECT_EQ(acc.count, 1u);
    EXPECT_EQ(acc.frames, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(FrameAccumulatorTest, LeavesTrailingPartialFrameUnconsumedForNextChunk) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2});
    buf.push_back(9);
    buf.push_back(0);  // dangling length prefix with no payload yet (more fread() needed)

    Accumulator acc;
    auto result = acc.run(buf, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/1000);

    EXPECT_FALSE(result.limitReached);
    EXPECT_EQ(acc.count, 1u);
    // Only the first, complete frame's 4 bytes (2-byte prefix + 2-byte
    // payload) were consumed - the dangling 2-byte prefix at the tail
    // was left alone for the caller to prepend the next chunk to.
    EXPECT_EQ(result.consumedBytes, 4u);
}

TEST(FrameAccumulatorTest, ResumingAcrossChunksCompletesASplitFrame) {
    // Simulates Journal::readFrom()'s loop: first chunk ends mid-frame,
    // second chunk supplies the rest, then the caller re-runs on the
    // concatenated (post-erase) buffer.
    std::vector<uint8_t> full;
    appendFrame(full, {1, 2, 3, 4, 5});

    std::vector<uint8_t> firstChunk(full.begin(), full.begin() + 3);  // prefix + first byte of payload
    std::vector<uint8_t> secondChunk(full.begin() + 3, full.end());

    Accumulator acc;
    auto result1 = acc.run(firstChunk, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/1000);
    EXPECT_FALSE(result1.limitReached);
    EXPECT_EQ(result1.consumedBytes, 0u);  // nothing complete yet
    EXPECT_EQ(acc.count, 0u);

    std::vector<uint8_t> pending = firstChunk;
    pending.insert(pending.end(), secondChunk.begin(), secondChunk.end());
    auto result2 = acc.run(pending, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/1000);
    EXPECT_FALSE(result2.limitReached);
    EXPECT_EQ(acc.count, 1u);
    EXPECT_EQ(acc.frames, (std::vector<uint8_t>{1, 2, 3, 4, 5}));
}

TEST(FrameAccumulatorTest, EmptyBufferConsumesNothing) {
    Accumulator acc;
    auto result = acc.run({}, /*nextWantedSeq=*/0, /*maxBytes=*/1000, /*maxRecords=*/1000);
    EXPECT_FALSE(result.limitReached);
    EXPECT_EQ(result.consumedBytes, 0u);
    EXPECT_EQ(acc.count, 0u);
    EXPECT_FALSE(acc.started);
}
