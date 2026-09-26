#ifndef CRONIOT_LOG_JOURNAL_H
#define CRONIOT_LOG_JOURNAL_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "JournalCursor.h"
#include "JournalTypes.h"
#include "LogRecord.h"
#include "WriteAccounting.h"

namespace croniot::log {

// One closed-or-open rotation segment's bookkeeping, as persisted in each
// stream's index file. Namespace-scope (not nested in Journal) purely so
// Journal.cpp's on-disk IndexFileFormat can name it directly.
struct SegmentRecord {
    uint32_t segmentId = 0;
    uint32_t oldestSeq = 0;
    uint32_t newestSeq = 0;
    uint32_t sizeBytes = 0;
    uint32_t closedAtUptimeSec = 0;  // 0 while still open
};

// Durable, append-only local storage for the `logs`/`events`/`data`
// streams (plan §11.3/§12.3): two LittleFS partitions ("journal" for
// Logs+Events, "archive" for Data - see §8.1), 64 KB rotating segments,
// persisted per-stream cursors, and space reclamation under pressure.
//
// Never fails hard. A partition that can't be found (e.g. built against
// a firmware whose partitions.csv predates the PR4 re-layout) just
// leaves that mount's streams disabled - appendLog()/appendEvent() and
// maybeReclaim() become no-ops for them, exactly the "degrades to
// solo-RAM, never tumba al firmware" contract from plan §8.8's last
// paragraph. This is also why Journal owns no static_assert-trivial
// globals of its own: everything it touches is a plain file, not
// `.noinit`/RTC memory - the ring upstream of it already carries that
// survivability, Journal is what drains the ring into something that
// outlives even a power-on reset.
//
// Known scoped gap, deliberately not solved here: the index file is only
// persisted at rotation boundaries (every 64 KB - see rotateIfNeeded() in
// Journal.cpp), never per record, matching the plan's "nunca en cada
// escritura" wear rule (§3.10/§8's WriteAccounting note). A crash between
// two rotations therefore reboots with a *stale* nextSeq for whichever
// segment was still open, and fresh post-reboot records can end up
// reusing `seq` values already physically written earlier in that same
// segment file. This is harmless under the plan's own dedup key
// `(deviceUuid, bootId, stream, seq)` - two records only collide if they
// also share `bootId`, and a crash always changes that - but Journal
// does not yet plumb a bootId through at all (Tanda D's uplink is the
// first consumer that actually needs one on the wire). Flagged here
// rather than solved now to avoid guessing at Tanda D's eventual bootId
// source before it exists.
class Journal {
public:
    static Journal& instance();

    // Idempotent. Mounts both partitions and loads each stream's index
    // file (segment list + cursor). Safe to call even if
    // CONFIG_CRONIOT_LOG_ENABLE is off at the Log.cpp layer - Log.cpp
    // simply never calls it in that case.
    void init();

    // Drain-task only (LogTask.cpp pops NoinitRing and calls this for
    // every record - both plain ESP_LOGx lines and rendered events end
    // up here, since both flow through the same ring; see LogRouter.cpp).
    // Assigns this record its own Logs-stream seq, encodes it, and
    // appends it to the current Logs segment, rotating/reclaiming as
    // needed. No-op if the `journal` partition isn't mounted.
    void appendLog(const LogRecord& record);

    // Any task - called synchronously from Log.cpp::event() in addition
    // to (not instead of) that event also flowing through the ring like
    // a normal line. Plan §8.9's "ERRORS.JSN es una copia, no un
    // movimiento" reasoning applies here too: the Events stream is a
    // durable, never-reclaimed copy specifically of structured events,
    // while the same event's rendering still appears in the ordinary
    // Logs timeline via the ring/drain path above. Blocking the calling
    // task on a flash write here is accepted - events are rare, and this
    // runs off the hook already (see Log.h's event() contract).
    void appendEvent(const LogRecord& record);

    // Runs SpaceReclaimer against whichever mounted partition(s) have
    // dropped under the low water mark. Called by LogTask periodically
    // and right after every rotation - never from the hot append path.
    void maybeReclaim();

    // Application-level ack (plan §5 point 3) - Tanda D's uplink will
    // call this once a server commit confirms `upToSeqInclusive`. A
    // no-op today (nothing calls it yet), kept here so PR12 compiles
    // against the final shape without another Journal change.
    void ack(Stream stream, uint32_t upToSeqInclusive);

    struct RawBatch {
        uint32_t firstSeq = 0;
        uint32_t count = 0;
        std::vector<uint8_t> frames;  // `count` raw, still-length-prefixed frames, concatenated verbatim
    };

    // Reads up to `maxRecords` records (capped at `maxBytes` of raw frame
    // data, prefixes included) starting at `fromSeqInclusive`, for the
    // uplink to send on (see telemetry/Uplink.cpp) - never decodes a
    // single one (see FrameCodec.h). `fromSeqInclusive` is clamped up to
    // whatever is actually still on disk: if the caller asks for a seq
    // that space reclamation already deleted, this starts from the
    // oldest surviving record instead (the resulting gap was already
    // reported via a GapMarker event when it happened - see
    // reclaimMount() - this just doesn't invent a *second*, silent one
    // by pretending deleted data is still there). Returns nullopt if the
    // stream isn't mounted or has nothing at or after `fromSeqInclusive`.
    std::optional<RawBatch> readFrom(Stream stream, uint32_t fromSeqInclusive, size_t maxBytes,
                                      size_t maxRecords);

    bool streamAvailable(Stream stream) const;
    uint32_t nextSeq(Stream stream) const;
    uint64_t lifetimeBytesWritten(Stream stream) const;

    // Where the uplink should resume sending from: one past the highest
    // acked seq, or 0 if nothing has ever been acked (including "no
    // server has ever been contacted yet"). Exposes exactly the cursor
    // state readFrom()'s caller needs without exposing JournalCursor
    // itself outside this class.
    uint32_t firstUnackedSeq(Stream stream) const;

    // Public so Journal.cpp's on-disk IndexFileFormat (an implementation
    // detail of that file, not exposed here) can size its segment array
    // to match exactly, instead of duplicating this number as a literal
    // that could silently drift out of sync.
    static constexpr uint32_t kMaxTrackedSegments = 64;

private:
    Journal() = default;

    static constexpr uint32_t kSegmentRotateBytes = 64 * 1024;
    static constexpr uint64_t kDataRetentionSeconds = 48 * 3600;  // plan §8.3: 24-72h default

    struct StreamState {
        bool mounted = false;
        const char* mountPath = nullptr;
        const char* partitionLabel = nullptr;
        const char* filePrefix = nullptr;
        JournalCursor cursor{Stream::Logs};  // stream_ overwritten in init()
        std::array<SegmentRecord, kMaxTrackedSegments> segments{};
        uint32_t segmentCount = 0;
        uint32_t openSegmentBytes = 0;
        void* openFile = nullptr;  // FILE*, opaque here to keep this header stdio-free
        uint32_t lastBudgetWarnDay = 0xFFFFFFFF;  // uptime-day of the last write_budget_exceeded event, so it fires once/day, not once per write
    };

    void initMount(Stream stream, StreamState& state, const char* mountPath,
                   const char* partitionLabel, const char* filePrefix);
    void loadIndex(StreamState& state);
    void saveIndex(const StreamState& state);
    void appendToStream(StreamState& state, Stream stream, const LogRecord& record);
    void rotateIfNeeded(StreamState& state, Stream stream);
    void reclaimMount(StreamState& state, const char* partitionLabel,
                       uint64_t retentionSeconds);
    static std::string segmentPath(const StreamState& state, uint32_t segmentId);
    StreamState& stateFor(Stream stream);

    StreamState logsState_;
    StreamState eventsState_;
    StreamState dataState_;
    WriteAccounting writeAccounting_;
    bool initialized_ = false;
};

}  // namespace croniot::log

#endif
