#include <gtest/gtest.h>

#include "comm/BleChannelLogic.h"

using croniot::BleChannelLogic::parseStaticPasskey;
using croniot::BleChannelLogic::djb2Hash;

TEST(ParseStaticPasskey, ValidSixDigitPasskey) {
    uint32_t out = 0;
    EXPECT_TRUE(parseStaticPasskey("123456", out));
    EXPECT_EQ(out, 123456u);
}

TEST(ParseStaticPasskey, TooShortIsRejected) {
    uint32_t out = 0;
    EXPECT_FALSE(parseStaticPasskey("12345", out));
}

TEST(ParseStaticPasskey, TooLongIsRejected) {
    uint32_t out = 0;
    EXPECT_FALSE(parseStaticPasskey("1234567", out));
}

TEST(ParseStaticPasskey, NonNumericIsRejected) {
    uint32_t out = 0;
    EXPECT_FALSE(parseStaticPasskey("12a456", out));
}

TEST(ParseStaticPasskey, EmptyStringIsRejected) {
    uint32_t out = 0;
    EXPECT_FALSE(parseStaticPasskey("", out));
}

TEST(ParseStaticPasskey, LeadingZerosAreValid) {
    uint32_t out = 0;
    EXPECT_TRUE(parseStaticPasskey("000001", out));
    EXPECT_EQ(out, 1u);
}

TEST(ParseStaticPasskey, AllZerosIsValidButWeak) {
    // Degenerate case: passes the current 6-digit/<=999999 check.
    // Flagged as a design question (weak passkey), not "fixed" here.
    uint32_t out = 123;
    EXPECT_TRUE(parseStaticPasskey("000000", out));
    EXPECT_EQ(out, 0u);
}

TEST(ParseStaticPasskey, RejectedInputLeavesOutputUntouched) {
    uint32_t out = 42;
    EXPECT_FALSE(parseStaticPasskey("bad", out));
    EXPECT_EQ(out, 42u);
}

TEST(Djb2Hash, EmptyStringIsSeedValue) {
    EXPECT_EQ(djb2Hash(""), 5381u);
}

TEST(Djb2Hash, IsDeterministic) {
    EXPECT_EQ(djb2Hash("schema-v1"), djb2Hash("schema-v1"));
}

TEST(Djb2Hash, DifferentInputsGiveDifferentHashes) {
    EXPECT_NE(djb2Hash("schema-v1"), djb2Hash("schema-v2"));
}
