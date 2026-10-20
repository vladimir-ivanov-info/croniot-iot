#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "OtaManifest.h"

// Every decision the OTA flow takes, as pure functions over a persisted
// OtaRecord - no ESP-IDF, so all of it is covered by test_host. The
// platform side (OtaController.cpp) only feeds facts in (running version,
// "is the running image pending verification", uptime, "has the server
// answered") and carries out what comes back. Plan: documentation/plans/ota.md §5.
namespace croniot::ota {

enum class Phase : uint8_t {
    Idle = 0,
    // A new image was written and the device rebooted into it; it is not
    // confirmed yet.
    Installed = 1,
    // The new image could not reach the server within the deadline, so we
    // rolled back on purpose to see whether the previous one can: if it
    // can, the new image is to blame; if it can't, the fault is outside
    // the device (server, internet, router).
    Probing = 2,
};

struct BadRelease {
    std::string version;
    std::string requestId;
};

struct OtaRecord {
    Phase phase = Phase::Idle;
    std::string requestId;
    std::string targetVersion;
    std::string previousVersion;
    uint8_t attempts = 0;
    // Why the last rollback we triggered ourselves happened, reported by
    // the previous image on the next boot. Empty when the bootloader
    // rolled back on its own (the new image crashed or reset).
    std::string lastFailure;
    // Releases this device refuses to install again. Keyed by request as
    // well as version: the server re-requesting the same version (a new
    // requestId) is the deliberate, manual way to retry it.
    std::vector<BadRelease> bad;
};

inline constexpr size_t kMaxBadReleases = 4;

// Failure reasons, as they appear in events.
inline constexpr const char* kReasonBoot = "boot";
inline constexpr const char* kReasonSelfTest = "self_test";
inline constexpr const char* kReasonNoConnectivity = "no_connectivity";

enum class ManifestDecision {
    Install,
    IgnoreSameVersion,
    // Still validating or probing a previous update.
    IgnoreBusy,
    IgnoreBad,
    IgnoreAttemptsExhausted,
};

ManifestDecision decideManifest(const OtaRecord& record, const Manifest& manifest,
                                const std::string& runningVersion, bool runningPendingVerify,
                                uint8_t maxAttempts);

// Call once the image is written, verified and set as the boot
// partition, right before restarting into it.
void recordInstalled(OtaRecord& record, const Manifest& manifest, const std::string& runningVersion);

bool isBad(const OtaRecord& record, const std::string& version, const std::string& requestId);
void markBad(OtaRecord& record, const std::string& version, const std::string& requestId);

enum class BootSituation {
    Normal,
    // Running the new image, which still has to prove itself.
    Validating,
    // The new image did not confirm and we are back on the previous one.
    RolledBack,
    // Back on the previous image on purpose, testing its connectivity.
    Probing,
};

struct BootOutcome {
    BootSituation situation = BootSituation::Normal;
    std::string reason;      // RolledBack only
    bool markedBad = false;  // RolledBack only: no attempts left
};

// Updates `record` for the image that just booted.
BootOutcome onBoot(OtaRecord& record, const std::string& runningVersion, uint8_t maxAttempts);

struct ValidationInputs {
    uint64_t uptimeMs = 0;
    bool selfTestsPassed = false;
    bool selfTestsFailed = false;
    bool serverReachable = false;
};

enum class ValidationVerdict { Wait, Confirm, RollbackSelfTest, RollbackNoConnectivity };

ValidationVerdict evaluateValidation(const ValidationInputs& in, uint64_t stableMs, uint64_t deadlineMs);

// Updates `record` before the controller rolls back on a verdict.
void recordRollback(OtaRecord& record, ValidationVerdict verdict);

void recordConfirmed(OtaRecord& record);

enum class ProbeVerdict {
    Wait,
    // The previous image reached the server within the deadline.
    NewVersionGuilty,
    // It only got through after the deadline: the outage was external.
    ExternalFault,
};

// `reachableAtMs` is the uptime at which the server first answered on
// this boot, 0 while it has not.
ProbeVerdict evaluateProbe(uint64_t reachableAtMs, uint64_t deadlineMs);

// Updates `record` with a final probe verdict. Returns true if the
// release ended up marked bad.
bool recordProbeResult(OtaRecord& record, ProbeVerdict verdict, uint8_t maxAttempts);

// NVS stores the record as one string blob.
std::string encodeRecord(const OtaRecord& record);
OtaRecord decodeRecord(const std::string& blob);

}  // namespace croniot::ota
