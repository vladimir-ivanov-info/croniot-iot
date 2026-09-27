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
