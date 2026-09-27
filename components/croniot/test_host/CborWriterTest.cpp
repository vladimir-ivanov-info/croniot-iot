#include <gtest/gtest.h>

#include "log/CborWriter.h"

using croniot::log::BatchHeader;
using croniot::log::encodeBatchHeader;
using croniot::log::encodeLogRecord;
using croniot::log::Level;
using croniot::log::LogRecord;

namespace {

// Golden bytes generated independently with Python's cbor2 library (not
// with this encoder), so this test actually verifies wire-format
// correctness rather than the encoder agreeing with itself.
std::vector<uint8_t> hexToBytes(const char* hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; hex[i] != '\0'; i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
    }
    return bytes;
}

}  // namespace

TEST(CborWriter, LogRecordMatchesGoldenBytes) {
    LogRecord record{};
    record.seq = 1234;
    record.uptimeMs = 5678;
    record.level = Level::Error;
    record.setTag("MyTag");
    record.setMessage("hello world");

    auto encoded = encodeLogRecord(record);
    auto expected = hexToBytes("851904d219162e00654d795461676b68656c6c6f20776f726c64");
    EXPECT_EQ(encoded, expected);
}

TEST(CborWriter, BatchHeaderWithoutEpochMatchesGoldenBytes) {
    BatchHeader header;
    header.version = 1;
    header.bootId = 7;
    header.firmwareVersion = "1.2.3";
    header.elfSha256Prefix = "abcd1234";
    header.nowMs = 987654321;
    header.dropped = 0;

    auto encoded = encodeBatchHeader(header);
    auto expected = hexToBytes(
        "a661760164626f6f740762667765312e322e3363656c66686162636431323334666e6f775f6d731a3ade68b16764726f7070656400");
    EXPECT_EQ(encoded, expected);
}

TEST(CborWriter, BatchHeaderWithEpochMatchesGoldenBytes) {
    BatchHeader header;
    header.version = 1;
    header.bootId = 7;
    header.firmwareVersion = "1.2.3";
    header.elfSha256Prefix = "abcd1234";
    header.nowMs = 987654321;
    header.epochMs = 1758888888000ULL;
    header.dropped = 3;

    auto encoded = encodeBatchHeader(header);
    auto expected = hexToBytes(
        "a761760164626f6f740762667765312e322e3363656c66686162636431323334666e6f775f6d731a3ade68b16865706f63685f6d73"
        "1b0000019985f286c06764726f7070656403");
    EXPECT_EQ(encoded, expected);
}

TEST(CborWriter, SmallUnsignedUsesSingleByteEncoding) {
    croniot::log::CborWriter writer;
    writer.writeUnsigned(5);
    EXPECT_EQ(writer.bytes(), (std::vector<uint8_t>{0x05}));
}

TEST(CborWriter, NegativeIntegerUsesMajorType1) {
    croniot::log::CborWriter writer;
    writer.writeSigned(-10);
    // CBOR negative encoding: value = -1 - n -> n = 9 = 0x09, major type 1.
    EXPECT_EQ(writer.bytes(), (std::vector<uint8_t>{0x29}));
}
