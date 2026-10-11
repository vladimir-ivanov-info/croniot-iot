#include "log/FrameCodec.h"

#include <gtest/gtest.h>
#include <vector>

using croniot::log::FrameCodec;

namespace {

void appendFrame(std::vector<uint8_t>& buf, const std::vector<uint8_t>& payload) {
    uint16_t len = static_cast<uint16_t>(payload.size());
    buf.push_back(static_cast<uint8_t>(len & 0xFF));
    buf.push_back(static_cast<uint8_t>(len >> 8));
    buf.insert(buf.end(), payload.begin(), payload.end());
}

}  // namespace

TEST(FrameCodecTest, ReadsSingleFrame) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2, 3});

    size_t offset = 0, start = 0, len = 0;
    ASSERT_TRUE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(start, 2u);
    EXPECT_EQ(len, 3u);
    EXPECT_EQ(offset, buf.size());
}

TEST(FrameCodecTest, ReadsMultipleFramesSequentially) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2});
    appendFrame(buf, {3, 4, 5});
    appendFrame(buf, {});

    size_t offset = 0, start = 0, len = 0;
    ASSERT_TRUE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(len, 2u);
    ASSERT_TRUE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(len, 3u);
    ASSERT_TRUE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(len, 0u);
    EXPECT_FALSE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
}

TEST(FrameCodecTest, EmptyBufferHasNoFrames) {
    size_t offset = 0, start = 0, len = 0;
    EXPECT_FALSE(FrameCodec::readNextFrame(nullptr, 0, offset, start, len));
}

TEST(FrameCodecTest, TruncatedLengthPrefixIsNotAFrame) {
    std::vector<uint8_t> buf = {5};  // only 1 byte, need 2 for the prefix
    size_t offset = 0, start = 0, len = 0;
    EXPECT_FALSE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(offset, 0u);  // unchanged on failure
}

TEST(FrameCodecTest, TruncatedPayloadIsNotAFrame) {
    // Claims a 10-byte payload but only 3 bytes actually follow - as if a
    // crash cut the write off mid-frame.
    std::vector<uint8_t> buf = {10, 0, 'a', 'b', 'c'};
    size_t offset = 0, start = 0, len = 0;
    EXPECT_FALSE(FrameCodec::readNextFrame(buf.data(), buf.size(), offset, start, len));
    EXPECT_EQ(offset, 0u);
}

TEST(FrameCodecTest, CountFramesMatchesNumberAppended) {
    std::vector<uint8_t> buf;
    for (int i = 0; i < 7; ++i) appendFrame(buf, {static_cast<uint8_t>(i)});
    EXPECT_EQ(FrameCodec::countFrames(buf.data(), buf.size()), 7u);
}

TEST(FrameCodecTest, CountFramesIgnoresTrailingPartialFrame) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2, 3});
    buf.push_back(9);
    buf.push_back(0);  // dangling length prefix, no payload
    EXPECT_EQ(FrameCodec::countFrames(buf.data(), buf.size()), 1u);
}

using croniot::log::FrameScanner;

namespace {

void feedInChunks(FrameScanner& scanner, const std::vector<uint8_t>& buf, size_t chunk) {
    for (size_t i = 0; i < buf.size(); i += chunk) {
        size_t n = buf.size() - i < chunk ? buf.size() - i : chunk;
        scanner.feed(buf.data() + i, n);
    }
}

}  // namespace

TEST(FrameScannerTest, EmptyInputHasNoFrames) {
    FrameScanner scanner;
    EXPECT_EQ(scanner.completeFrames(), 0u);
    EXPECT_EQ(scanner.validBytes(), 0u);
    EXPECT_EQ(scanner.totalBytes(), 0u);
}

TEST(FrameScannerTest, CountsCompleteFramesFedAtOnce) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2});
    appendFrame(buf, {3, 4, 5});
    appendFrame(buf, {6});

    FrameScanner scanner;
    scanner.feed(buf.data(), buf.size());
    EXPECT_EQ(scanner.completeFrames(), 3u);
    EXPECT_EQ(scanner.validBytes(), buf.size());
    EXPECT_EQ(scanner.totalBytes(), buf.size());
}

// The boot-time reader feeds fixed-size chunks that split frames - and
// even a frame's 2-byte length prefix - at arbitrary points.
TEST(FrameScannerTest, SameResultWhateverTheChunkSize) {
    std::vector<uint8_t> buf;
    for (uint8_t i = 0; i < 20; ++i) appendFrame(buf, std::vector<uint8_t>(i * 7 + 1, i));

    for (size_t chunk : {size_t{1}, size_t{2}, size_t{3}, size_t{7}, size_t{256}, buf.size()}) {
        FrameScanner scanner;
        feedInChunks(scanner, buf, chunk);
        EXPECT_EQ(scanner.completeFrames(), 20u) << "chunk=" << chunk;
        EXPECT_EQ(scanner.validBytes(), buf.size()) << "chunk=" << chunk;
    }
}

TEST(FrameScannerTest, PartialTrailingPayloadIsNotCounted) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2, 3});
    size_t validEnd = buf.size();
    appendFrame(buf, {4, 5, 6, 7});
    buf.resize(buf.size() - 2);  // power cut mid-payload

    FrameScanner scanner;
    scanner.feed(buf.data(), buf.size());
    EXPECT_EQ(scanner.completeFrames(), 1u);
    EXPECT_EQ(scanner.validBytes(), validEnd);
    EXPECT_EQ(scanner.totalBytes(), buf.size());
}

TEST(FrameScannerTest, PartialTrailingPrefixIsNotCounted) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {1, 2, 3});
    size_t validEnd = buf.size();
    buf.push_back(9);  // only one byte of the next length prefix made it

    FrameScanner scanner;
    scanner.feed(buf.data(), buf.size());
    EXPECT_EQ(scanner.completeFrames(), 1u);
    EXPECT_EQ(scanner.validBytes(), validEnd);
    EXPECT_EQ(scanner.totalBytes(), validEnd + 1);
}

TEST(FrameScannerTest, ZeroLengthPayloadCountsAsAFrameLikeReadNextFrame) {
    std::vector<uint8_t> buf;
    appendFrame(buf, {});
    appendFrame(buf, {1});

    FrameScanner scanner;
    scanner.feed(buf.data(), buf.size());
    EXPECT_EQ(scanner.completeFrames(), FrameCodec::countFrames(buf.data(), buf.size()));
    EXPECT_EQ(scanner.validBytes(), buf.size());
}
