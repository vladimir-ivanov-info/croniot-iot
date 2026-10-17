#ifndef CRONIOT_LOG_JOURNALCURSOR_H
#define CRONIOT_LOG_JOURNALCURSOR_H

#include <cstdint>
#include <optional>

#include "JournalTypes.h"

namespace croniot::log {

// Pure sequence-number bookkeeping for a single journal stream (see
// plan §5): who has written up to where (`nextSeq`), who the server has
// confirmed up to (`ackedSeq`), and who is still physically on flash
// (`oldestSeq`). No file I/O, no clock - Journal.h drives this from real
// writes/acks/reclaims; tests drive it directly.
//
// `ackedSeq` uses -1-as-"none yet" via a bool flag rather than an
// optional<uint32_t> plus sentinel math, matching the plan's own framing
// ("ackedSeq: hasta donde ha confirmado el servidor" - nothing, at boot,
// with no server yet contacted).
class JournalCursor {
public:
    explicit JournalCursor(Stream stream) : stream_(stream) {}

    // Assigns and returns the next sequence number, advancing nextSeq().
    uint32_t assignSeq() { return nextSeq_++; }

    uint32_t nextSeq() const { return nextSeq_; }
    uint32_t oldestSeq() const { return oldestSeq_; }
    bool hasAck() const { return hasAck_; }
    uint32_t ackedSeq() const { return ackedSeq_; }  // only meaningful if hasAck()

    // Restores persisted state (e.g. read back from the cursor file at
    // boot) without going through assignSeq()/ack() semantics.
    void restore(uint32_t nextSeq, uint32_t oldestSeq, std::optional<uint32_t> ackedSeq) {
        nextSeq_ = nextSeq;
        oldestSeq_ = oldestSeq;
        hasAck_ = ackedSeq.has_value();
        ackedSeq_ = ackedSeq.value_or(0);
    }

    // Application-level ack (plan §5 point 3) - never moves backwards,
    // so a stale/duplicate ack from a reconnect can't un-confirm data.
    // Never moves past what was written either: the server acks the seq
    // it read inside the records, so a batch carrying stale records with
    // higher seqs (old segment files from before an index reset) would
    // otherwise mark records not yet written as confirmed, and the stream
    // would look empty until nextSeq caught up.
    void ack(uint32_t upToSeqInclusive) {
        if (nextSeq_ == 0) return;  // nothing written: there is nothing to confirm
        if (upToSeqInclusive >= nextSeq_) upToSeqInclusive = nextSeq_ - 1;
        if (!hasAck_ || upToSeqInclusive > ackedSeq_) {
            hasAck_ = true;
            ackedSeq_ = upToSeqInclusive;
        }
    }

    // Called when the space reclaimer deletes everything older than
    // `newOldestSeq` (plan §8.8). If that range includes records the
    // server never acked, returns the GapMarker to report - the honesty
    // mechanism from §5 point 8 ("huecos explícitos"), never a silent
    // hole. Returns nullopt when the reclaimed range was fully acked
    // already (the common, non-newsworthy case).
    std::optional<GapMarker> reclaimTo(uint32_t newOldestSeq, const char* reason) {
        uint32_t previousOldest = oldestSeq_;
        if (newOldestSeq <= previousOldest) return std::nullopt;  // nothing new reclaimed

        uint32_t unackedBoundary = hasAck_ ? ackedSeq_ + 1 : 0;  // first seq NOT yet acked
        oldestSeq_ = newOldestSeq;

        uint32_t gapFrom = previousOldest > unackedBoundary ? previousOldest : unackedBoundary;
        if (gapFrom >= newOldestSeq) return std::nullopt;  // the reclaimed range was fully acked

        return GapMarker{stream_, gapFrom, newOldestSeq, newOldestSeq - gapFrom, reason};
    }

private:
    Stream stream_;
    uint32_t nextSeq_ = 0;
    uint32_t oldestSeq_ = 0;
    bool hasAck_ = false;
    uint32_t ackedSeq_ = 0;
};

}  // namespace croniot::log

#endif
