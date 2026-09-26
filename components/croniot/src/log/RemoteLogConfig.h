#ifndef CRONIOT_LOG_REMOTELOGCONFIG_H
#define CRONIOT_LOG_REMOTELOGCONFIG_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Level.h"

namespace croniot::log {

struct RemoteLogConfig {
    std::optional<Level> defaultLevel;
    std::vector<std::pair<std::string, Level>> tagLevels;
    uint32_t ttlSec = 0;
};

// Parses this SDK's own reduced remote log-config shape - a deliberate
// scoping-down of the plan's full local/remote/sink-detail matrix
// (§3.4) to just what LevelResolver (already built in the logging-core
// batch) can act on today: a top-level default level, per-tag
// overrides, and one TTL that applies to both. Per-sink control
// (flash/sd on/off, detail Compact/Normal/Full) stays code/Kconfig-only
// for now - see Log.h's applyRemoteConfig() for the full list of what's
// deferred.
//
//   {"default":"warn","tags":{"WifiMqttController":"trace"},"ttlSec":600}
//
// All three keys are optional; an empty object is valid and changes
// nothing. Unknown keys are skipped, not rejected (forward-compatible
// with a server that adds fields an older firmware doesn't know about
// yet - matches kmp's own `ignoreUnknownKeys` Json config). Hand-rolled
// rather than pulling in a general JSON library (same reasoning as
// CborWriter/BatchEnvelope elsewhere in this SDK, and CborReader on the
// kmp side): the shape is small, fixed, and never grows arbitrary
// nesting, so a real JSON parser would be answering a much bigger
// question than this one actually asks - and cJSON, the one JSON
// library already in this SDK's REQUIRES, isn't available to
// test_host's plain-g++ build the way this pure-logic file needs to be.
// Returns nullopt on malformed JSON, a malformed known-key value, or an
// unrecognized level name - never partially applies a bad config.
std::optional<RemoteLogConfig> parseRemoteLogConfig(const std::string& json);

}  // namespace croniot::log

#endif
