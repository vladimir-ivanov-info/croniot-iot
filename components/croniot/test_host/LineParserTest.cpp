#include <gtest/gtest.h>

#include <cstring>

#include "log/LineParser.h"
#include "log/LogRecord.h"

using croniot::log::Level;
using croniot::log::LogRecord;
using croniot::log::parseEspLogLine;
using croniot::log::parseEspLogLineFast;

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

// parseEspLogLineFast() is the zero-allocation on-device counterpart (see
// LineParser.h) - same v1-line grammar, mirrored test coverage, but
// writing into a LogRecord's fixed buffers instead of returning a
// heap-allocated ParsedLine.

TEST(LineParserFast, ParsesPlainUncoloredLine) {
    const char* line = "I (1234) MyTag: hello world\n";
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), record));
    EXPECT_EQ(record.level, Level::Info);
    EXPECT_EQ(record.uptimeMs, 1234u);
    EXPECT_STREQ(record.tag, "MyTag");
    EXPECT_STREQ(record.message, "hello world");
}

TEST(LineParserFast, ParsesColoredLine) {
    // Real tag name from the codebase, and real production behavior: at
    // 19 chars it already exceeds LogRecord::kMaxTagLen (15) and gets
    // truncated - true for the pre-existing slow hot path too (it also
    // wrote through LogRecord::setTag()), not something this change
    // introduces. See TruncatesTagAndMessageToLogRecordLimits below for
    // dedicated truncation coverage.
    const char* line = "\033[0;31mE (99) WifiMqttController: disconnected\033[0m\n";
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), record));
    EXPECT_EQ(record.level, Level::Error);
    EXPECT_EQ(record.uptimeMs, 99u);
    EXPECT_STREQ(record.tag, "WifiMqttControl");
    EXPECT_STREQ(record.message, "disconnected");
}

TEST(LineParserFast, MapsEspIdfVerboseToTrace) {
    const char* line = "V (0) Boot: verbose line";
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), record));
    EXPECT_EQ(record.level, Level::Trace);
}

TEST(LineParserFast, HandlesLineWithoutTrailingNewline) {
    const char* line = "D (42) Tag: no newline here";
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), record));
    EXPECT_STREQ(record.message, "no newline here");
}

TEST(LineParserFast, RejectsLineWithNoTagSeparator) {
    const char* line = "plain printf with no tag\n";
    LogRecord record{};
    EXPECT_FALSE(parseEspLogLineFast(line, std::strlen(line), record));
}

TEST(LineParserFast, RejectsEmptyLine) {
    LogRecord record{};
    EXPECT_FALSE(parseEspLogLineFast("", 0, record));
}

TEST(LineParserFast, RejectsMissingUptimeDigits) {
    const char* line = "I () Tag: message";
    LogRecord record{};
    EXPECT_FALSE(parseEspLogLineFast(line, std::strlen(line), record));
}

TEST(LineParserFast, MessageCanContainColonSpace) {
    const char* line = "I (1) Tag: key: value pairs: still one message\n";
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), record));
    EXPECT_STREQ(record.tag, "Tag");
    EXPECT_STREQ(record.message, "key: value pairs: still one message");
}

TEST(LineParserFast, TruncatesTagAndMessageToLogRecordLimits) {
    std::string longTag(64, 'T');
    std::string longMessage(400, 'M');
    std::string line = "I (1) " + longTag + ": " + longMessage;
    LogRecord record{};
    ASSERT_TRUE(parseEspLogLineFast(line.data(), line.size(), record));
    EXPECT_EQ(std::strlen(record.tag), croniot::log::kMaxTagLen);
    EXPECT_EQ(std::strlen(record.message), croniot::log::kMaxMessageLen);
}

TEST(LineParserFast, AgreesWithParseEspLogLineOnSameInput) {
    const char* line = "W (555) SomeTag: something happened here\n";
    auto slow = parseEspLogLine(line);
    LogRecord fast{};
    ASSERT_TRUE(parseEspLogLineFast(line, std::strlen(line), fast));
    ASSERT_TRUE(slow.valid);
    EXPECT_EQ(fast.level, slow.level);
    EXPECT_EQ(fast.uptimeMs, slow.uptimeMs);
    EXPECT_STREQ(fast.tag, slow.tag.c_str());
    EXPECT_STREQ(fast.message, slow.message.c_str());
}
