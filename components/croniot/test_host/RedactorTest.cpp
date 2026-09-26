#include <gtest/gtest.h>

#include "log/Redactor.h"

using croniot::log::Redactor;

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
