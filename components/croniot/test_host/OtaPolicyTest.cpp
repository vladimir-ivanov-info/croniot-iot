#include "ota/OtaPolicy.h"

#include <gtest/gtest.h>

using namespace croniot::ota;

namespace {

constexpr uint8_t kMaxAttempts = 2;
constexpr uint64_t kStableMs = 60 * 1000;
constexpr uint64_t kDeadlineMs = 30 * 60 * 1000;

Manifest manifest(const std::string& version = "2.0", const std::string& requestId = "req-1") {
    Manifest m;
    m.requestId = requestId;
    m.version = version;
    m.path = "/api/iot/firmware/1";
    m.size = 100;
    m.sha256 = std::string(64, 'a');
    return m;
}

// Simulates "download, verify, reboot into the new image".
OtaRecord installed(const std::string& from = "1.0", const Manifest& m = manifest()) {
    OtaRecord r;
    recordInstalled(r, m, from);
    return r;
}

}  // namespace

TEST(OtaPolicyTest, InstallsANewVersion) {
    OtaRecord r;
    EXPECT_EQ(decideManifest(r, manifest(), "1.0", false, kMaxAttempts), ManifestDecision::Install);
}

TEST(OtaPolicyTest, IgnoresTheVersionAlreadyRunning) {
    OtaRecord r;
    EXPECT_EQ(decideManifest(r, manifest("1.0"), "1.0", false, kMaxAttempts), ManifestDecision::IgnoreSameVersion);
}

TEST(OtaPolicyTest, DoesNotStartAnUpdateWhileTheCurrentOneIsUnconfirmed) {
    OtaRecord r;
    EXPECT_EQ(decideManifest(r, manifest("3.0"), "2.0", /*pendingVerify=*/true, kMaxAttempts),
              ManifestDecision::IgnoreBusy);
    OtaRecord probing = installed();
    probing.phase = Phase::Probing;
    EXPECT_EQ(decideManifest(probing, manifest("3.0"), "1.0", false, kMaxAttempts), ManifestDecision::IgnoreBusy);
}

TEST(OtaPolicyTest, BootIntoTheNewImageStartsValidation) {
    OtaRecord r = installed();
    EXPECT_EQ(r.attempts, 1);
    EXPECT_EQ(r.previousVersion, "1.0");
    BootOutcome out = onBoot(r, "2.0", kMaxAttempts);
    EXPECT_EQ(out.situation, BootSituation::Validating);
    EXPECT_EQ(r.phase, Phase::Installed);
}

TEST(OtaPolicyTest, ACrashedImageIsRetriedOnceThenMarkedBad) {
    OtaRecord r = installed();
    // The bootloader rolled back by itself: we are on 1.0 again.
    BootOutcome first = onBoot(r, "1.0", kMaxAttempts);
    EXPECT_EQ(first.situation, BootSituation::RolledBack);
    EXPECT_EQ(first.reason, kReasonBoot);
    EXPECT_FALSE(first.markedBad);
    EXPECT_EQ(r.phase, Phase::Idle);

    // The retained manifest arrives again: second and last attempt.
    ASSERT_EQ(decideManifest(r, manifest(), "1.0", false, kMaxAttempts), ManifestDecision::Install);
    recordInstalled(r, manifest(), "1.0");
    EXPECT_EQ(r.attempts, 2);
    BootOutcome second = onBoot(r, "1.0", kMaxAttempts);
    EXPECT_TRUE(second.markedBad);
    EXPECT_EQ(decideManifest(r, manifest(), "1.0", false, kMaxAttempts), ManifestDecision::IgnoreBad);
}

TEST(OtaPolicyTest, ANewRequestForABadVersionIsTheManualRetry) {
    OtaRecord r;
    markBad(r, "2.0", "req-1");
    EXPECT_EQ(decideManifest(r, manifest("2.0", "req-1"), "1.0", false, kMaxAttempts), ManifestDecision::IgnoreBad);
    EXPECT_EQ(decideManifest(r, manifest("2.0", "req-2"), "1.0", false, kMaxAttempts), ManifestDecision::Install);
    recordInstalled(r, manifest("2.0", "req-2"), "1.0");
    EXPECT_EQ(r.attempts, 1);
}

TEST(OtaPolicyTest, ASelfTestFailureIsReportedByThePreviousImage) {
    OtaRecord r = installed();
    onBoot(r, "2.0", kMaxAttempts);
    ValidationInputs in;
    in.uptimeMs = 5000;
    in.selfTestsFailed = true;
    ValidationVerdict v = evaluateValidation(in, kStableMs, kDeadlineMs);
    ASSERT_EQ(v, ValidationVerdict::RollbackSelfTest);
    recordRollback(r, v);

    BootOutcome back = onBoot(r, "1.0", kMaxAttempts);
    EXPECT_EQ(back.situation, BootSituation::RolledBack);
    EXPECT_EQ(back.reason, kReasonSelfTest);
}

TEST(OtaPolicyTest, ConfirmsOnlyOnceStableHealthyAndReachable) {
    ValidationInputs in;
    in.selfTestsPassed = true;
    in.serverReachable = true;
    in.uptimeMs = kStableMs - 1;
    EXPECT_EQ(evaluateValidation(in, kStableMs, kDeadlineMs), ValidationVerdict::Wait);
    in.uptimeMs = kStableMs;
    EXPECT_EQ(evaluateValidation(in, kStableMs, kDeadlineMs), ValidationVerdict::Confirm);
    in.serverReachable = false;
    EXPECT_EQ(evaluateValidation(in, kStableMs, kDeadlineMs), ValidationVerdict::Wait);
}

TEST(OtaPolicyTest, ConfirmationResetsTheRecord) {
    OtaRecord r = installed();
    onBoot(r, "2.0", kMaxAttempts);
    recordConfirmed(r);
    EXPECT_EQ(r.phase, Phase::Idle);
    EXPECT_EQ(r.attempts, 0);
    EXPECT_EQ(decideManifest(r, manifest(), "2.0", false, kMaxAttempts), ManifestDecision::IgnoreSameVersion);
}

TEST(OtaPolicyTest, NoConnectivityAtTheDeadlineProbesThePreviousImage) {
    OtaRecord r = installed();
    onBoot(r, "2.0", kMaxAttempts);
    ValidationInputs in;
    in.selfTestsPassed = true;
    in.uptimeMs = kDeadlineMs;
    ValidationVerdict v = evaluateValidation(in, kStableMs, kDeadlineMs);
    ASSERT_EQ(v, ValidationVerdict::RollbackNoConnectivity);
    recordRollback(r, v);
    EXPECT_EQ(r.phase, Phase::Probing);

    BootOutcome back = onBoot(r, "1.0", kMaxAttempts);
    EXPECT_EQ(back.situation, BootSituation::Probing);
}

TEST(OtaPolicyTest, PreviousImageConnectingInTimeBlamesTheNewOne) {
    OtaRecord r = installed();
    recordRollback(r, ValidationVerdict::RollbackNoConnectivity);
    onBoot(r, "1.0", kMaxAttempts);

    EXPECT_EQ(evaluateProbe(0, kDeadlineMs), ProbeVerdict::Wait);
    ProbeVerdict v = evaluateProbe(/*reachableAtMs=*/20 * 1000, kDeadlineMs);
    ASSERT_EQ(v, ProbeVerdict::NewVersionGuilty);
    EXPECT_TRUE(recordProbeResult(r, v, kMaxAttempts));
    EXPECT_EQ(r.phase, Phase::Idle);
    EXPECT_EQ(decideManifest(r, manifest(), "1.0", false, kMaxAttempts), ManifestDecision::IgnoreBad);
}

TEST(OtaPolicyTest, AnExternalOutageDoesNotCondemnTheNewVersion) {
    OtaRecord r = installed();
    recordRollback(r, ValidationVerdict::RollbackNoConnectivity);
    onBoot(r, "1.0", kMaxAttempts);

    // The previous image only got through after the deadline as well.
    ProbeVerdict v = evaluateProbe(kDeadlineMs + 1, kDeadlineMs);
    ASSERT_EQ(v, ProbeVerdict::ExternalFault);
    EXPECT_FALSE(recordProbeResult(r, v, kMaxAttempts));
    EXPECT_EQ(decideManifest(r, manifest(), "1.0", false, kMaxAttempts), ManifestDecision::Install);
}

TEST(OtaPolicyTest, RepeatedExternalOutagesStillStopAfterMaxAttempts) {
    OtaRecord r = installed();
    recordRollback(r, ValidationVerdict::RollbackNoConnectivity);
    onBoot(r, "1.0", kMaxAttempts);
    recordProbeResult(r, ProbeVerdict::ExternalFault, kMaxAttempts);

    recordInstalled(r, manifest(), "1.0");
    recordRollback(r, ValidationVerdict::RollbackNoConnectivity);
    onBoot(r, "1.0", kMaxAttempts);
    EXPECT_TRUE(recordProbeResult(r, ProbeVerdict::ExternalFault, kMaxAttempts));
}

TEST(OtaPolicyTest, ProbingThatDidNotRollBackKeepsValidating) {
    OtaRecord r = installed();
    recordRollback(r, ValidationVerdict::RollbackNoConnectivity);
    BootOutcome out = onBoot(r, "2.0", kMaxAttempts);
    EXPECT_EQ(out.situation, BootSituation::Validating);
    EXPECT_EQ(r.phase, Phase::Installed);
}

TEST(OtaPolicyTest, BadListIsCapped) {
    OtaRecord r;
    for (int i = 0; i < 10; ++i) markBad(r, "v" + std::to_string(i), "r");
    EXPECT_EQ(r.bad.size(), kMaxBadReleases);
    EXPECT_EQ(r.bad.front().version, "v9");  // most recent first
}

TEST(OtaPolicyTest, RecordRoundTrips) {
    OtaRecord r = installed();
    r.lastFailure = kReasonSelfTest;
    markBad(r, "0.9", "old");
    markBad(r, "0.8", "older");
    OtaRecord back = decodeRecord(encodeRecord(r));
    EXPECT_EQ(back.phase, r.phase);
    EXPECT_EQ(back.requestId, r.requestId);
    EXPECT_EQ(back.targetVersion, r.targetVersion);
    EXPECT_EQ(back.previousVersion, r.previousVersion);
    EXPECT_EQ(back.attempts, r.attempts);
    EXPECT_EQ(back.lastFailure, r.lastFailure);
    ASSERT_EQ(back.bad.size(), 2u);
    EXPECT_EQ(back.bad[0].version, "0.8");
    EXPECT_EQ(back.bad[1].requestId, "old");
}

TEST(OtaPolicyTest, GarbageDecodesToAnEmptyRecord) {
    EXPECT_EQ(decodeRecord("").phase, Phase::Idle);
    EXPECT_EQ(decodeRecord("v2\n1\n").phase, Phase::Idle);
    EXPECT_EQ(decodeRecord("v1\n9\na\nb\nc\n1\n\n0\n").phase, Phase::Idle);
    EXPECT_TRUE(decodeRecord("v1\n1\na\nb\nc\n1\n\n3\nx\ty\n").requestId.empty());
}
