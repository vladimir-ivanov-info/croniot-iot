#include "log/SegmentFiles.h"

#include <gtest/gtest.h>

using croniot::log::SegmentFiles;

TEST(SegmentFilesTest, ParsesThisStreamsSegmentFiles) {
    EXPECT_EQ(SegmentFiles::parseSegmentId("logs_00000.cbor", "logs"), 0u);
    EXPECT_EQ(SegmentFiles::parseSegmentId("logs_00042.cbor", "logs"), 42u);
    EXPECT_EQ(SegmentFiles::parseSegmentId("events_123456.cbor", "events"), 123456u);
}

TEST(SegmentFilesTest, IgnoresOtherStreamsThatShareThePartition) {
    // Logs and Events live in the same "journal" partition.
    EXPECT_FALSE(SegmentFiles::parseSegmentId("events_00001.cbor", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_00001.cbor", "events").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logsx_00001.cbor", "logs").has_value());
}

TEST(SegmentFilesTest, IgnoresTheIndexAndAnythingElse) {
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_index.bin", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_index.tmp", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_.cbor", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_00001.cbor.tmp", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_00001", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs", "logs").has_value());
    EXPECT_FALSE(SegmentFiles::parseSegmentId("logs_99999999999.cbor", "logs").has_value());  // > uint32
}
