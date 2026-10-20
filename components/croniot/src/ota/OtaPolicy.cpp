#include "OtaPolicy.h"

#include <algorithm>
#include <cstdlib>

namespace croniot::ota {

ManifestDecision decideManifest(const OtaRecord& record, const Manifest& manifest,
                                const std::string& runningVersion, bool runningPendingVerify,
                                uint8_t maxAttempts) {
    if (manifest.version == runningVersion) return ManifestDecision::IgnoreSameVersion;
    if (runningPendingVerify || record.phase != Phase::Idle) return ManifestDecision::IgnoreBusy;
    if (isBad(record, manifest.version, manifest.requestId)) return ManifestDecision::IgnoreBad;
    bool sameRequest = record.requestId == manifest.requestId && record.targetVersion == manifest.version;
    if (sameRequest && record.attempts >= maxAttempts) return ManifestDecision::IgnoreAttemptsExhausted;
    return ManifestDecision::Install;
}

void recordInstalled(OtaRecord& record, const Manifest& manifest, const std::string& runningVersion) {
    bool sameRequest = record.requestId == manifest.requestId && record.targetVersion == manifest.version;
    if (!sameRequest) record.attempts = 0;
    if (record.attempts < UINT8_MAX) ++record.attempts;
    record.requestId = manifest.requestId;
    record.targetVersion = manifest.version;
    record.previousVersion = runningVersion;
    record.lastFailure.clear();
    record.phase = Phase::Installed;
}

bool isBad(const OtaRecord& record, const std::string& version, const std::string& requestId) {
    return std::any_of(record.bad.begin(), record.bad.end(), [&](const BadRelease& b) {
        return b.version == version && b.requestId == requestId;
    });
}

void markBad(OtaRecord& record, const std::string& version, const std::string& requestId) {
    if (isBad(record, version, requestId)) return;
    record.bad.insert(record.bad.begin(), BadRelease{version, requestId});
    if (record.bad.size() > kMaxBadReleases) record.bad.resize(kMaxBadReleases);
}

BootOutcome onBoot(OtaRecord& record, const std::string& runningVersion, uint8_t maxAttempts) {
    BootOutcome out;
    switch (record.phase) {
        case Phase::Idle:
            return out;

        case Phase::Installed:
            if (runningVersion == record.targetVersion) {
                out.situation = BootSituation::Validating;
                return out;
            }
            // Back on another image without having confirmed: either the
            // bootloader rolled back by itself (no lastFailure) or we did.
            out.situation = BootSituation::RolledBack;
            out.reason = record.lastFailure.empty() ? kReasonBoot : record.lastFailure;
            if (record.attempts >= maxAttempts) {
                markBad(record, record.targetVersion, record.requestId);
                out.markedBad = true;
            }
            record.lastFailure.clear();
            record.phase = Phase::Idle;
            return out;

        case Phase::Probing:
            if (runningVersion == record.targetVersion) {
                // The rollback we asked for did not happen (no previous
                // valid image): keep validating what we have.
                record.phase = Phase::Installed;
                out.situation = BootSituation::Validating;
                return out;
            }
            out.situation = BootSituation::Probing;
            return out;
    }
    return out;
}

ValidationVerdict evaluateValidation(const ValidationInputs& in, uint64_t stableMs, uint64_t deadlineMs) {
    if (in.selfTestsFailed) return ValidationVerdict::RollbackSelfTest;
    if (in.uptimeMs >= stableMs && in.selfTestsPassed && in.serverReachable) return ValidationVerdict::Confirm;
    if (in.uptimeMs >= deadlineMs) {
        // Reachable but not yet past the stability window can't happen
        // here as long as deadline > stable; anything else unconfirmed at
        // the deadline is a connectivity failure.
        return in.serverReachable ? ValidationVerdict::Wait : ValidationVerdict::RollbackNoConnectivity;
    }
    return ValidationVerdict::Wait;
}

void recordRollback(OtaRecord& record, ValidationVerdict verdict) {
    if (verdict == ValidationVerdict::RollbackSelfTest) {
        record.lastFailure = kReasonSelfTest;
        record.phase = Phase::Installed;
    } else if (verdict == ValidationVerdict::RollbackNoConnectivity) {
        record.lastFailure = kReasonNoConnectivity;
        record.phase = Phase::Probing;
    }
}

void recordConfirmed(OtaRecord& record) {
    record.phase = Phase::Idle;
    record.lastFailure.clear();
    record.attempts = 0;
}

ProbeVerdict evaluateProbe(uint64_t reachableAtMs, uint64_t deadlineMs) {
    if (reachableAtMs == 0) return ProbeVerdict::Wait;
    return reachableAtMs <= deadlineMs ? ProbeVerdict::NewVersionGuilty : ProbeVerdict::ExternalFault;
}

bool recordProbeResult(OtaRecord& record, ProbeVerdict verdict, uint8_t maxAttempts) {
    if (verdict == ProbeVerdict::Wait) return false;
    bool bad = verdict == ProbeVerdict::NewVersionGuilty || record.attempts >= maxAttempts;
    if (bad) markBad(record, record.targetVersion, record.requestId);
    record.phase = Phase::Idle;
    record.lastFailure.clear();
    return bad;
}

// One field per line; bad releases as "version\trequestId" lines after a
// count. Versions and request ids never contain '\n' or '\t' (the server
// generates UUIDs; versions come from PROJECT_VER), and decodeRecord()
// falls back to an empty record on anything it can't read.
std::string encodeRecord(const OtaRecord& r) {
    std::string out = "v1\n";
    out += std::to_string(static_cast<int>(r.phase)) + "\n";
    out += r.requestId + "\n";
    out += r.targetVersion + "\n";
    out += r.previousVersion + "\n";
    out += std::to_string(r.attempts) + "\n";
    out += r.lastFailure + "\n";
    out += std::to_string(r.bad.size()) + "\n";
    for (const auto& b : r.bad) out += b.version + "\t" + b.requestId + "\n";
    return out;
}

OtaRecord decodeRecord(const std::string& blob) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < blob.size()) {
        size_t nl = blob.find('\n', start);
        if (nl == std::string::npos) return {};
        lines.push_back(blob.substr(start, nl - start));
        start = nl + 1;
    }
    if (lines.size() < 8 || lines[0] != "v1") return {};

    OtaRecord r;
    int phase = std::atoi(lines[1].c_str());
    if (phase < 0 || phase > static_cast<int>(Phase::Probing)) return {};
    r.phase = static_cast<Phase>(phase);
    r.requestId = lines[2];
    r.targetVersion = lines[3];
    r.previousVersion = lines[4];
    r.attempts = static_cast<uint8_t>(std::min(std::atoi(lines[5].c_str()), 255));
    r.lastFailure = lines[6];
    size_t badCount = static_cast<size_t>(std::atoi(lines[7].c_str()));
    if (lines.size() != 8 + badCount || badCount > kMaxBadReleases) return {};
    for (size_t i = 0; i < badCount; ++i) {
        const std::string& line = lines[8 + i];
        size_t tab = line.find('\t');
        if (tab == std::string::npos) return {};
        r.bad.push_back(BadRelease{line.substr(0, tab), line.substr(tab + 1)});
    }
    return r;
}

}  // namespace croniot::ota
