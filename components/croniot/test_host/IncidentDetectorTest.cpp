#include "log/IncidentDetector.h"

#include <gtest/gtest.h>

using croniot::log::IncidentSummary;
using croniot::log::isCrashLikeReset;
using croniot::log::ResetCause;
using croniot::log::shouldRaiseIncident;

TEST(IncidentDetectorTest, PanicAndWatchdogsAreCrashLike) {
    EXPECT_TRUE(isCrashLikeReset(ResetCause::Panic));
    EXPECT_TRUE(isCrashLikeReset(ResetCause::TaskWdt));
    EXPECT_TRUE(isCrashLikeReset(ResetCause::InterruptWdt));
    EXPECT_TRUE(isCrashLikeReset(ResetCause::OtherWdt));
    EXPECT_TRUE(isCrashLikeReset(ResetCause::SoftwareRestart));
}

TEST(IncidentDetectorTest, PowerOnBrownoutAndDeepSleepAreNotCrashLike) {
    EXPECT_FALSE(isCrashLikeReset(ResetCause::PowerOn));
    EXPECT_FALSE(isCrashLikeReset(ResetCause::Brownout));
    EXPECT_FALSE(isCrashLikeReset(ResetCause::DeepSleepWake));
}

TEST(IncidentDetectorTest, CleanRestartWithNoEvidenceIsNotAnIncident) {
    IncidentSummary summary{ResetCause::SoftwareRestart, 0, 0, false};
    EXPECT_FALSE(shouldRaiseIncident(summary));
}

TEST(IncidentDetectorTest, PanicWithLeftoverRingRecordsIsAnIncident) {
    IncidentSummary summary{ResetCause::Panic, /*recoveredRingRecords=*/3, 0, false};
    EXPECT_TRUE(shouldRaiseIncident(summary));
}

TEST(IncidentDetectorTest, PanicWithLeftoverRtcRecordsIsAnIncident) {
    IncidentSummary summary{ResetCause::Panic, 0, /*recoveredRtcRecords=*/1, false};
    EXPECT_TRUE(shouldRaiseIncident(summary));
}

TEST(IncidentDetectorTest, PanicWithCoredumpIsAnIncidentEvenWithEmptyBuffers) {
    IncidentSummary summary{ResetCause::Panic, 0, 0, /*coredumpPresent=*/true};
    EXPECT_TRUE(shouldRaiseIncident(summary));
}

TEST(IncidentDetectorTest, PowerOnWithLeftoverDataIsNotAnIncident) {
    // By the time this runs, NoinitRing::init()/RtcCriticalStore::init()
    // will already have reset themselves on a real power-on (magic/CRC
    // won't validate), so recoveredRingRecords should be 0 in practice -
    // but the detector itself must not raise an incident from a
    // non-crash-like cause regardless, as a second independent guard.
    IncidentSummary summary{ResetCause::PowerOn, /*recoveredRingRecords=*/3, 0, false};
    EXPECT_FALSE(shouldRaiseIncident(summary));
}
