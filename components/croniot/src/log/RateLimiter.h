#ifndef CRONIOT_LOG_RATELIMITER_H
#define CRONIOT_LOG_RATELIMITER_H

#include <optional>
#include <string>

#include "LogRecord.h"

namespace croniot::log {

// Collapses a run of identical (tag, message) records into a single
// "repeated Nx" summary instead of dropping any of them: the first
// occurrence is emitted immediately, the following identical ones are
// counted, and a distinct record (or an explicit flush) releases a summary
// record with repeatCount set to how many were absorbed. This is
// compression, not filtering - a run's count is exact, and no record with
// different content is ever suppressed.
class RateLimiter {
public:
    struct FeedResult {
        bool emitNow = true;
        // If set, a run of `pendingFlush->repeatCount` earlier duplicates
        // must be emitted (as one "repeated Nx" line) before/alongside the
        // fed record.
        std::optional<LogRecord> pendingFlush;
    };

    FeedResult feed(const LogRecord& record) {
        FeedResult result;

        if (hasLast_ && lastTag_ == record.tag && lastMessage_ == record.message &&
            lastLevel_ == record.level) {
            ++repeatCount_;
            result.emitNow = false;
            return result;
        }

        if (repeatCount_ > 0) {
            result.pendingFlush = makeFlushRecord();
        }

        hasLast_ = true;
        lastTag_ = record.tag;
        lastMessage_ = record.message;
        lastLevel_ = record.level;
        repeatCount_ = 0;
        result.emitNow = true;
        return result;
    }

    // Releases a still-open run even though nothing different has arrived
    // yet (e.g. called periodically by the drain loop so a stuck repeat
    // isn't held forever waiting for a change).
    std::optional<LogRecord> flushPending() {
        if (repeatCount_ == 0) return std::nullopt;
        auto record = makeFlushRecord();
        repeatCount_ = 0;
        return record;
    }

private:
    LogRecord makeFlushRecord() const {
        LogRecord record{};
        record.level = lastLevel_;
        record.setTag(lastTag_.c_str());
        record.setMessage(lastMessage_.c_str());
        record.repeatCount = repeatCount_;
        return record;
    }

    bool hasLast_ = false;
    std::string lastTag_;
    std::string lastMessage_;
    Level lastLevel_ = Level::Info;
    uint16_t repeatCount_ = 0;
};

}  // namespace croniot::log

#endif
