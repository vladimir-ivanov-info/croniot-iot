#include <gtest/gtest.h>

#include <cstring>

#include "log/Redactor.h"

using croniot::log::Redactor;

namespace {
// redactInPlace() takes a mutable, NUL-terminated buffer of known
// capacity - this builds one from a string literal for the tests below.
template <size_t N>
struct MutableBuf {
    char data[N];
    explicit MutableBuf(const char* initial) { std::strncpy(data, initial, N - 1); data[N - 1] = '\0'; }
};
}  // namespace

TEST(Redactor, PassesThroughWhenNoSecretsRegistered) {
    Redactor redactor;
    EXPECT_EQ(redactor.redact("nothing sensitive here"), "nothing sensitive here");
}

TEST(Redactor, ReplacesRegisteredSecretWithAsterisks) {
    Redactor redactor;
    redactor.registerSecret("abc123");
    EXPECT_EQ(redactor.redact("token=abc123 ok"), "token=*** ok");
}

TEST(Redactor, ReplacesAllOccurrencesAndAllRegisteredSecrets) {
    Redactor redactor;
    redactor.registerSecret("secretA");
    redactor.registerSecret("secretB");
    EXPECT_EQ(redactor.redact("secretA then secretB then secretA again"), "*** then *** then *** again");
}

TEST(Redactor, EmptySecretIsIgnored) {
    Redactor redactor;
    redactor.registerSecret("");
    EXPECT_EQ(redactor.redact("unchanged"), "unchanged");
}

TEST(Redactor, ClearRemovesAllRegisteredSecrets) {
    Redactor redactor;
    redactor.registerSecret("gone");
    redactor.clear();
    EXPECT_EQ(redactor.redact("gone but not redacted"), "gone but not redacted");
}

TEST(Redactor, LeavesUnrelatedTextAlone) {
    Redactor redactor;
    redactor.registerSecret("xyz");
    EXPECT_EQ(redactor.redact("nothing to see"), "nothing to see");
}

// redactInPlace() is the zero-allocation on-device counterpart (see
// Redactor.h) - same masking behavior, mirrored test coverage, but
// mutating a fixed buffer instead of returning a new std::string.

TEST(RedactorInPlace, PassesThroughWhenNoSecretsRegistered) {
    Redactor redactor;
    MutableBuf<64> buf("nothing sensitive here");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "nothing sensitive here");
}

TEST(RedactorInPlace, ReplacesRegisteredSecretWithAsterisks) {
    Redactor redactor;
    redactor.registerSecret("abc123");
    MutableBuf<64> buf("token=abc123 ok");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "token=*** ok");
}

TEST(RedactorInPlace, ReplacesAllOccurrencesAndAllRegisteredSecrets) {
    Redactor redactor;
    redactor.registerSecret("secretA");
    redactor.registerSecret("secretB");
    MutableBuf<64> buf("secretA then secretB then secretA again");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "*** then *** then *** again");
}

TEST(RedactorInPlace, EmptySecretIsIgnored) {
    Redactor redactor;
    redactor.registerSecret("");
    MutableBuf<64> buf("unchanged");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "unchanged");
}

TEST(RedactorInPlace, ClearRemovesAllRegisteredSecrets) {
    Redactor redactor;
    redactor.registerSecret("gone");
    redactor.clear();
    MutableBuf<64> buf("gone but not redacted");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "gone but not redacted");
}

TEST(RedactorInPlace, LeavesUnrelatedTextAlone) {
    Redactor redactor;
    redactor.registerSecret("xyz");
    MutableBuf<64> buf("nothing to see");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, "nothing to see");
}

TEST(RedactorInPlace, AgreesWithRedactOnSameInput) {
    Redactor redactor;
    redactor.registerSecret("deadbeef1234");
    std::string expected = redactor.redact("device token=deadbeef1234 connected");
    MutableBuf<64> buf("device token=deadbeef1234 connected");
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    EXPECT_STREQ(buf.data, expected.c_str());
}

TEST(RedactorInPlace, ShorterSecretThanMaskClampsInsteadOfOverflowing) {
    // "ab" (2 chars) replaced by "***" (3 chars) grows the string by one
    // byte per match - with the buffer already at capacity, that growth
    // must clamp rather than write past bufSize.
    Redactor redactor;
    redactor.registerSecret("ab");
    MutableBuf<8> buf("xxxabxx");  // 7 chars + NUL == exactly the 8-byte capacity
    redactor.redactInPlace(buf.data, sizeof(buf.data));
    // Must not overflow (ASan/host would abort on a real out-of-bounds
    // write); the exact clamped content matters less than staying in bounds.
    EXPECT_EQ(std::strlen(buf.data) + 1, sizeof(buf.data));
}
