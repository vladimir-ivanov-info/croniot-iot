#include <cstdint>
#include <string>

#include "gtest/gtest.h"

#include "health/HealthReport.h"

using croniot::health::encodeMemNet;
using croniot::health::encodeStorage;
using croniot::health::kNoPercent;
using croniot::health::kNoRssi;
using croniot::health::MemNetInputs;
using croniot::health::StorageInputs;

// LogRecord::message caps at 200 bytes (kMaxMessageLen, see LogRecord.h)
// and silently truncates past that - the whole reason these two events
// are split apart instead of one big blob is to stay well under that cap
// even at worst-case field widths. This is the guarantee that matters,
// not any particular byte count.
constexpr size_t kLogRecordMessageCap = 200;

TEST(HealthReportTest, EncodeMemNetTypicalValues) {
    MemNetInputs in;
    in.freeHeapBytes = 123456;
    in.largestFreeBlockBytes = 98765;
    in.uptimeSeconds = 864000;
    in.rssi = -67;
    in.wifiReconnects = 3;
    in.logsDropped = 0;

    EXPECT_EQ(encodeMemNet(in), "heap=123456 blk=98765 up=864000 rssi=-67 recon=3 drop=0");
}

TEST(HealthReportTest, EncodeMemNetNotConnectedOmitsRssi) {
    MemNetInputs in;
    in.rssi = kNoRssi;
    EXPECT_NE(encodeMemNet(in).find("rssi=na"), std::string::npos);
}

TEST(HealthReportTest, EncodeMemNetWorstCaseFitsUnderLogRecordCap) {
    MemNetInputs in;
    in.freeHeapBytes = UINT32_MAX;
    in.largestFreeBlockBytes = UINT32_MAX;
    in.uptimeSeconds = UINT64_MAX;
    in.rssi = -128;
    in.wifiReconnects = UINT32_MAX;
    in.logsDropped = UINT32_MAX;

    EXPECT_LT(encodeMemNet(in).size(), kLogRecordMessageCap);
}

TEST(HealthReportTest, EncodeStorageTypicalValues) {
    StorageInputs in;
    in.journalFreePercent = 82;
    in.archiveFreePercent = 95;
    in.journalWearPercent = 1;
    in.archiveWearPercent = 0;
    in.backlogCount = {0, 0, 0};
    in.resendAttempts = {0, 0, 0};

    EXPECT_EQ(encodeStorage(in),
              "jfree=82 afree=95 jwear=1 awear=0 lbl=0 ebl=0 dbl=0 lrs=0 ers=0 drs=0");
}

TEST(HealthReportTest, EncodeStorageUnmountedPartitionIsNa) {
    StorageInputs in;  // defaults: everything kNoPercent
    std::string encoded = encodeStorage(in);
    EXPECT_NE(encoded.find("jfree=na"), std::string::npos);
    EXPECT_NE(encoded.find("afree=na"), std::string::npos);
    EXPECT_NE(encoded.find("jwear=na"), std::string::npos);
    EXPECT_NE(encoded.find("awear=na"), std::string::npos);
}

TEST(HealthReportTest, EncodeStorageWorstCaseFitsUnderLogRecordCap) {
    StorageInputs in;
    in.journalFreePercent = 100;
    in.archiveFreePercent = 100;
    in.journalWearPercent = 100;
    in.archiveWearPercent = 100;
    in.backlogCount = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    in.resendAttempts = {UINT32_MAX, UINT32_MAX, UINT32_MAX};

    EXPECT_LT(encodeStorage(in).size(), kLogRecordMessageCap);
}
