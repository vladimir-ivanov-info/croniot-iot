#include <gtest/gtest.h>

#include "log/LineParser.h"

using croniot::log::Level;
using croniot::log::parseEspLogLine;

TEST(LineParser, ParsesPlainUncoloredLine) {
    auto parsed = parseEspLogLine("I (1234) MyTag: hello world\n");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.level, Level::Info);
    EXPECT_EQ(parsed.uptimeMs, 1234u);
    EXPECT_EQ(parsed.tag, "MyTag");
    EXPECT_EQ(parsed.message, "hello world");
}

TEST(LineParser, ParsesColoredLine) {
    auto parsed = parseEspLogLine("\033[0;31mE (99) WifiMqttController: disconnected\033[0m\n");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.level, Level::Error);
    EXPECT_EQ(parsed.uptimeMs, 99u);
    EXPECT_EQ(parsed.tag, "WifiMqttController");
    EXPECT_EQ(parsed.message, "disconnected");
}

TEST(LineParser, MapsEspIdfVerboseToTrace) {
    auto parsed = parseEspLogLine("V (0) Boot: verbose line");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.level, Level::Trace);
}

TEST(LineParser, HandlesLineWithoutTrailingNewline) {
    auto parsed = parseEspLogLine("D (42) Tag: no newline here");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.message, "no newline here");
}

TEST(LineParser, RejectsLineWithNoTagSeparator) {
    auto parsed = parseEspLogLine("plain printf with no tag\n");
    EXPECT_FALSE(parsed.valid);
}

TEST(LineParser, RejectsEmptyLine) {
    auto parsed = parseEspLogLine("");
    EXPECT_FALSE(parsed.valid);
}

TEST(LineParser, RejectsMissingUptimeDigits) {
    auto parsed = parseEspLogLine("I () Tag: message");
    EXPECT_FALSE(parsed.valid);
}

TEST(LineParser, MessageCanContainColonSpace) {
    auto parsed = parseEspLogLine("I (1) Tag: key: value pairs: still one message\n");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.tag, "Tag");
    EXPECT_EQ(parsed.message, "key: value pairs: still one message");
}
