#include "log/RemoteLogConfig.h"

#include <gtest/gtest.h>

using croniot::log::Level;
using croniot::log::parseRemoteLogConfig;

TEST(RemoteLogConfigTest, ParsesFullConfig) {
    auto config = parseRemoteLogConfig(R"({"default":"warn","tags":{"WifiMqttController":"trace","Uplink":"debug"},"ttlSec":600})");
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(config->defaultLevel.has_value());
    EXPECT_EQ(*config->defaultLevel, Level::Warn);
    ASSERT_EQ(config->tagLevels.size(), 2u);
    EXPECT_EQ(config->tagLevels[0].first, "WifiMqttController");
    EXPECT_EQ(config->tagLevels[0].second, Level::Trace);
    EXPECT_EQ(config->tagLevels[1].first, "Uplink");
    EXPECT_EQ(config->tagLevels[1].second, Level::Debug);
    EXPECT_EQ(config->ttlSec, 600u);
}

TEST(RemoteLogConfigTest, EmptyObjectIsValidAndChangesNothing) {
    auto config = parseRemoteLogConfig("{}");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->defaultLevel.has_value());
    EXPECT_TRUE(config->tagLevels.empty());
    EXPECT_EQ(config->ttlSec, 0u);
}

TEST(RemoteLogConfigTest, DefaultOnlyNoTtl) {
    auto config = parseRemoteLogConfig(R"({"default":"error"})");
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(*config->defaultLevel, Level::Error);
    EXPECT_EQ(config->ttlSec, 0u);
}

TEST(RemoteLogConfigTest, TagsOnlyNoDefault) {
    auto config = parseRemoteLogConfig(R"({"tags":{"TaskWaterPlants":"info"}})");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->defaultLevel.has_value());
    ASSERT_EQ(config->tagLevels.size(), 1u);
    EXPECT_EQ(config->tagLevels[0].first, "TaskWaterPlants");
}

TEST(RemoteLogConfigTest, UnknownKeyIsSkippedNotRejected) {
    auto config = parseRemoteLogConfig(R"({"immediateFrom":"error","default":"debug","future":{"nested":[1,2,3]}})");
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(*config->defaultLevel, Level::Debug);
}

TEST(RemoteLogConfigTest, UnknownLevelNameFails) {
    EXPECT_FALSE(parseRemoteLogConfig(R"({"default":"verbose"})").has_value());
}

TEST(RemoteLogConfigTest, MalformedJsonFails) {
    EXPECT_FALSE(parseRemoteLogConfig(R"({"default":"warn")").has_value());
    EXPECT_FALSE(parseRemoteLogConfig("not json").has_value());
    EXPECT_FALSE(parseRemoteLogConfig("").has_value());
}

TEST(RemoteLogConfigTest, NegativeTtlFails) {
    EXPECT_FALSE(parseRemoteLogConfig(R"({"ttlSec":-5})").has_value());
}

TEST(RemoteLogConfigTest, EscapedStringInTagNameIsHandled) {
    auto config = parseRemoteLogConfig(R"({"tags":{"Tag\"With\"Quotes":"trace"}})");
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->tagLevels.size(), 1u);
    EXPECT_EQ(config->tagLevels[0].first, "Tag\"With\"Quotes");
}
