#include "Sensors/SensorBatchEncoder.h"

#include <gtest/gtest.h>

using croniot::encodeSensorBatch;
using croniot::SensorSample;

namespace {
std::string toHex(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0x0F];
    }
    return out;
}
}  // namespace

// Golden bytes independently generated with Python's struct module
// (not this SDK's own CborWriter) from
// [42, 1000, 60000, [21.5, 22.0, 21.8]] - same "verify against an
// independent encoder" discipline as BatchEnvelopeTest.cpp.
TEST(SensorBatchEncoderTest, MatchesIndependentCborGoldenBytes) {
    std::vector<SensorSample> samples = {{0, 21.5}, {0, 22.0}, {0, 21.8}};
    auto bytes = encodeSensorBatch(/*sensorUid=*/42, /*t0Ms=*/1000, /*dtMs=*/60000, samples);

    EXPECT_EQ(toHex(bytes),
              "84182a1903e819ea6083fb4035800000000000fb4036000000000000fb4035cccccccccccd");
}

TEST(SensorBatchEncoderTest, EmptySamplesEncodesAnEmptyArray) {
    auto bytes = encodeSensorBatch(1, 0, 0, {});
    // [1, 0, 0, []]
    EXPECT_EQ(toHex(bytes), "8401000080");
}
