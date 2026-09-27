#ifndef CRONIOT_LOG_LEVELRESOLVER_H
#define CRONIOT_LOG_LEVELRESOLVER_H

#include <map>
#include <optional>
#include <string>

#include "Level.h"

namespace croniot::log {

// Resolves the effective capture level for a tag, honoring the four
// configuration origins of the plan (each overrides the previous):
//   1. Kconfig default (compiled in)
//   2. Code (CroniotConfig.log / croniot::log::setLevel with no ttl)
//   3. Remote, persisted config (/littlefs/log_config.json)
//   4. Temporary TTL override (RAM working cache; caller is responsible for
//      the durable/RTC copies described in the plan - this class only
//      knows "is it still valid at this instant").
//
// Every query takes an explicit `nowMs` instead of reading a clock, so this
// stays deterministic and host-testable; the platform layer supplies real
// time (uptime, or epoch once SNTP is available).
class LevelResolver {
public:
    void setKconfigDefault(Level level) { kconfigDefault_ = level; }

    void setCodeDefault(Level level) { codeDefault_ = level; }
    void setCodeTagLevel(const std::string& tag, Level level) { codeTags_[tag] = level; }

    void setRemoteDefault(Level level) { remoteDefault_ = level; }
    void setRemoteTagLevel(const std::string& tag, Level level) { remoteTags_[tag] = level; }
    void clearRemote() {
        remoteDefault_.reset();
        remoteTags_.clear();
    }

    // A TTL override with no tag applies to every tag that has no more
    // specific TTL override of its own.
    void setTtlOverride(Level level, uint64_t expiresAtMs) { ttlDefault_ = {level, expiresAtMs}; }
    void setTtlTagOverride(const std::string& tag, Level level, uint64_t expiresAtMs) {
        ttlTags_[tag] = {level, expiresAtMs};
    }
    void clearTtlOverrides() {
        ttlDefault_.reset();
        ttlTags_.clear();
    }

    Level effectiveLevel(const std::string& tag, uint64_t nowMs) const {
        // A tag-specific TTL wins while valid; once it lapses, fall back to
        // a still-valid blanket TTL rather than straight to the persisted
        // layers - an expired per-tag override shouldn't block a broader
        // one that's still running.
        if (auto it = ttlTags_.find(tag); it != ttlTags_.end() && it->second.expiresAtMs > nowMs) {
            return it->second.level;
        }
        if (ttlDefault_ && ttlDefault_->expiresAtMs > nowMs) {
            return ttlDefault_->level;
        }

        if (auto it = remoteTags_.find(tag); it != remoteTags_.end()) return it->second;
        if (auto it = codeTags_.find(tag); it != codeTags_.end()) return it->second;

        if (remoteDefault_) return *remoteDefault_;
        if (codeDefault_) return *codeDefault_;
        return kconfigDefault_;
    }

private:
    struct TtlEntry {
        Level level;
        uint64_t expiresAtMs;
    };

    Level kconfigDefault_ = Level::Info;
    std::optional<Level> codeDefault_;
    std::map<std::string, Level> codeTags_;
    std::optional<Level> remoteDefault_;
    std::map<std::string, Level> remoteTags_;
    std::optional<TtlEntry> ttlDefault_;
    std::map<std::string, TtlEntry> ttlTags_;
};

}  // namespace croniot::log

#endif
