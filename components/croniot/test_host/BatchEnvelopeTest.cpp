#include "telemetry/BatchEnvelope.h"

#include <gtest/gtest.h>

#include "log/CborWriter.h"

using croniot::log::encodeLogRecord;
using croniot::log::LogRecord;
using croniot::log::Level;
using croniot::log::Stream;
using croniot::telemetry::encodeBatchEnvelope;

namespace {

LogRecord makeRecord(uint32_t seq, uint64_t uptimeMs, Level level, const char* tag,
                      const char* message) {
    LogRecord record{};
    record.seq = seq;
    record.uptimeMs = uptimeMs;
    record.level = level;
    record.setTag(tag);
    record.setMessage(message);
    return record;
}

}  // namespace

// Golden bytes independently produced by Python's cbor2 (not this
// codebase's CborWriter) for:
//   [7, 0, 1, 2, [[1,1000,0,"TAG","hello"], [2,2000,1,"TAG","world"]]]
// i.e. bootId=7, stream=Logs(0), firstSeq=1, count=2, two records -
// cross-checking the *outer envelope* the same way CborWriterTest.cpp
// already cross-checks individual records.
TEST(BatchEnvelopeTest, MatchesIndependentCborGoldenBytes) {
    LogRecord rec1 = makeRecord(1, 1000, Level::Error, "TAG", "hello");
    LogRecord rec2 = makeRecord(2, 2000, Level::Warn, "TAG", "world");

    std::vector<uint8_t> raw;
    auto bytes1 = encodeLogRecord(rec1);
    auto bytes2 = encodeLogRecord(rec2);
    raw.insert(raw.end(), bytes1.begin(), bytes1.end());
    raw.insert(raw.end(), bytes2.begin(), bytes2.end());

    auto batch = encodeBatchEnvelope(/*bootId=*/7, Stream::Logs, /*firstSeq=*/1, /*count=*/2, raw);

    std::vector<uint8_t> expected = {
        0x85, 0x07, 0x00, 0x01, 0x02, 0x82, 0x85, 0x01, 0x19, 0x03, 0xe8, 0x00, 0x63, 0x54,
        0x41, 0x47, 0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x85, 0x02, 0x19, 0x07, 0xd0, 0x01,
        0x63, 0x54, 0x41, 0x47, 0x65, 0x77, 0x6f, 0x72, 0x6c, 0x64,
    };
    EXPECT_EQ(batch, expected);
}

TEST(BatchEnvelopeTest, EmptyBatchHasZeroCountAndEmptyRecordsArray) {
    auto batch = encodeBatchEnvelope(1, Stream::Events, 0, 0, {});
    // array(5): bootId=1, stream=1(Events), firstSeq=0, count=0, records=array(0)
    std::vector<uint8_t> expected = {0x85, 0x01, 0x01, 0x00, 0x00, 0x80};
    EXPECT_EQ(batch, expected);
}

TEST(BatchEnvelopeTest, StreamIndexMatchesEnumOrdinal) {
    auto logs = encodeBatchEnvelope(0, Stream::Logs, 0, 0, {});
    auto events = encodeBatchEnvelope(0, Stream::Events, 0, 0, {});
    auto data = encodeBatchEnvelope(0, Stream::Data, 0, 0, {});
    EXPECT_EQ(logs[2], 0x00);
    EXPECT_EQ(events[2], 0x01);
    EXPECT_EQ(data[2], 0x02);
}
