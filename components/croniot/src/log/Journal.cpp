#include "Journal.h"

#include <unistd.h>

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "CborWriter.h"
#include "FrameAccumulator.h"
#include "FrameCodec.h"
#include "SpaceReclaimer.h"

namespace croniot::log {

namespace {

constexpr const char* TAG = "Journal";
constexpr uint32_t kIndexMagic = 0x4A524E4C;  // 'LNRJ' little-endian ascii
constexpr uint32_t kIndexVersion = 1;

#ifdef CONFIG_CRONIOT_LOG_JOURNAL_LOW_WATER_MARK_PERCENT
constexpr double kLowWaterMarkPercent = CONFIG_CRONIOT_LOG_JOURNAL_LOW_WATER_MARK_PERCENT;
#else
constexpr double kLowWaterMarkPercent = 20.0;
#endif

#ifdef CONFIG_CRONIOT_LOG_JOURNAL_CRITICAL_MARK_PERCENT
constexpr double kCriticalMarkPercent = CONFIG_CRONIOT_LOG_JOURNAL_CRITICAL_MARK_PERCENT;
#else
constexpr double kCriticalMarkPercent = 5.0;
#endif

#ifdef CONFIG_CRONIOT_LOG_JOURNAL_MAX_BYTES_PER_DAY
constexpr uint32_t kMaxBytesPerDay = CONFIG_CRONIOT_LOG_JOURNAL_MAX_BYTES_PER_DAY;
#else
constexpr uint32_t kMaxBytesPerDay = 0;  // 0 = no budget (plan: hardBudget off by default)
#endif

uint64_t uptimeSeconds() { return static_cast<uint64_t>(esp_timer_get_time() / 1000000); }

// No reliable wall clock exists yet (no SNTP - Fase 6). An uptime-based
// day index is exactly what the plan asks for in this situation (§3.4:
// "sin reloj se expresa como uptime acumulado"): it resets to 0 every
// boot, but that only ever makes the daily budget re-arm a little early
// after a reboot, never late - the safe direction to err (see
// WriteAccounting.h's Snapshot comment for the same reasoning).
uint32_t uptimeDayIndex() { return static_cast<uint32_t>(uptimeSeconds() / 86400); }

// Every on-disk record is a 2-byte little-endian length prefix followed
// by that many bytes of CBOR - simple enough to walk without a CBOR
// decoder (this codebase only has CborWriter, by design - see
// CborWriter.h), while still being unambiguous for a future tool that
// wants to read a segment file back.
bool writeLengthPrefixed(FILE* file, const std::vector<uint8_t>& bytes) {
    uint16_t len = static_cast<uint16_t>(bytes.size());
    uint8_t header[2] = {static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>(len >> 8)};
    bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    ok = ok && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return fflush(file) == 0 && ok;
}

// Journal's own bookkeeping file, one per mount - NOT the wire/black-box
// format (LogRecord/CborWriter). Plain aggregate with default member
// initializers is fine here: unlike NoinitRingHeader/LogRecord, this
// lives in an ordinary file, never in `.noinit`/RTC memory, so there's
// no startup-constructor hazard to guard against.
struct IndexFileFormat {
    uint32_t magic = kIndexMagic;
    uint32_t version = kIndexVersion;
    uint32_t nextSeq = 0;
    uint32_t oldestSeq = 0;
    uint8_t hasAck = 0;
    uint32_t ackedSeq = 0;
    uint32_t segmentCount = 0;
    uint32_t openSegmentBytes = 0;
    uint32_t lastBudgetWarnDay = 0xFFFFFFFF;
    std::array<SegmentRecord, Journal::kMaxTrackedSegments> segments{};
};

}  // namespace

Journal& Journal::instance() {
    static Journal journal;
    return journal;
}

void Journal::init() {
    if (initialized_) return;
    initialized_ = true;

    logsState_.cursor = JournalCursor(Stream::Logs);
    eventsState_.cursor = JournalCursor(Stream::Events);
    dataState_.cursor = JournalCursor(Stream::Data);

    initMount(Stream::Logs, logsState_, "/journal", "journal", "logs");
    initMount(Stream::Events, eventsState_, "/journal", "journal", "events");
    initMount(Stream::Data, dataState_, "/archive", "archive", "data");
}

void Journal::initMount(Stream stream, StreamState& state, const char* mountPath,
                         const char* partitionLabel, const char* filePrefix) {
    state.mountPath = mountPath;
    state.partitionLabel = partitionLabel;
    state.filePrefix = filePrefix;

    // Logs and Events share the "journal" partition/mount point - only
    // register+mount it once (esp_vfs_littlefs_register returns
    // ESP_ERR_INVALID_STATE if a label is already mounted).
    if (stream == Stream::Events && logsState_.mounted &&
        std::strcmp(logsState_.partitionLabel, partitionLabel) == 0) {
        state.mounted = true;
        loadIndex(state);
        return;
    }

    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = mountPath;
    conf.partition_label = partitionLabel;
    conf.format_if_mount_failed = true;

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        // Never fatal: a firmware built against an older partitions.csv
        // (before PR4's "journal"/"archive" partitions existed) simply
        // runs with this stream's flash journaling disabled - the ring
        // still works, console still works, only durability past a
        // power-on reset is missing for this stream. See Journal.h.
        ESP_LOGW(TAG, "stream '%s' unavailable: could not mount partition '%s' (%s)",
                 toString(stream), partitionLabel, esp_err_to_name(err));
        state.mounted = false;
        return;
    }

    state.mounted = true;
    loadIndex(state);
}

void Journal::loadIndex(StreamState& state) {
    std::string path = std::string(state.mountPath) + "/" + state.filePrefix + "_index.bin";
    FILE* file = fopen(path.c_str(), "rb");

    IndexFileFormat index;
    bool valid = false;
    if (file) {
        valid = fread(&index, sizeof(index), 1, file) == 1 && index.magic == kIndexMagic &&
                index.version == kIndexVersion;
        fclose(file);
    }

    if (valid) {
        state.cursor.restore(index.nextSeq, index.oldestSeq,
                              index.hasAck ? std::optional<uint32_t>(index.ackedSeq) : std::nullopt);
        state.segmentCount = index.segmentCount > kMaxTrackedSegments ? kMaxTrackedSegments
                                                                       : index.segmentCount;
        state.openSegmentBytes = index.openSegmentBytes;
        state.lastBudgetWarnDay = index.lastBudgetWarnDay;
        for (uint32_t i = 0; i < state.segmentCount; ++i) {
            state.segments[i].segmentId = index.segments[i].segmentId;
            state.segments[i].oldestSeq = index.segments[i].oldestSeq;
            state.segments[i].newestSeq = index.segments[i].newestSeq;
            state.segments[i].sizeBytes = index.segments[i].sizeBytes;
            state.segments[i].closedAtUptimeSec = index.segments[i].closedAtUptimeSec;
        }
    }
    // If not valid (first boot ever, or a corrupt/missing index), start
    // this stream fresh at seq 0 with no tracked segments. Any raw
    // segment files left over from before are simply orphaned - they
    // stay on flash, untracked, until a human clears them; that's a far
    // safer failure mode than guessing at their contents.

    if (state.segmentCount == 0) {
        SegmentRecord first;
        first.segmentId = 0;
        first.oldestSeq = state.cursor.nextSeq();
        first.newestSeq = state.cursor.nextSeq();
        state.segments[0] = first;
        state.segmentCount = 1;
    }

    // The index is only saved on rotation/ack/reclaim, so its view of the
    // open segment is stale after any reboot - the file is the source of truth.
    recoverOpenSegment(state);
}

void Journal::saveIndex(const StreamState& state) {
    if (!state.mounted) return;

    IndexFileFormat index;
    index.nextSeq = state.cursor.nextSeq();
    index.oldestSeq = state.cursor.oldestSeq();
    index.hasAck = state.cursor.hasAck() ? 1 : 0;
    index.ackedSeq = state.cursor.ackedSeq();
    index.segmentCount = state.segmentCount;
    index.openSegmentBytes = state.openSegmentBytes;
    index.lastBudgetWarnDay = state.lastBudgetWarnDay;
    for (uint32_t i = 0; i < state.segmentCount; ++i) {
        index.segments[i].segmentId = state.segments[i].segmentId;
        index.segments[i].oldestSeq = state.segments[i].oldestSeq;
        index.segments[i].newestSeq = state.segments[i].newestSeq;
        index.segments[i].sizeBytes = state.segments[i].sizeBytes;
        index.segments[i].closedAtUptimeSec = state.segments[i].closedAtUptimeSec;
    }

    std::string path = std::string(state.mountPath) + "/" + state.filePrefix + "_index.bin";
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) {
        ESP_LOGW(TAG, "could not persist index at '%s'", path.c_str());
        return;
    }
    fwrite(&index, sizeof(index), 1, file);
    fclose(file);
}

bool Journal::writeFrame(StreamState& state, const std::vector<uint8_t>& bytes) {
    uint32_t openId = state.segments[state.segmentCount - 1].segmentId;
    if (!state.openFile) {
        std::string path = segmentPath(state, openId);
        state.openFile = fopen(path.c_str(), "ab");
        if (!state.openFile) {
            ESP_LOGW(TAG, "could not open segment '%s'", path.c_str());
            return false;
        }
    }

    if (writeLengthPrefixed(static_cast<FILE*>(state.openFile), bytes)) return true;

    // A half-written frame would misalign every frame appended after it, so
    // cut the file back to its last known-good length (e.g. partition full).
    std::string path = segmentPath(state, openId);
    ESP_LOGW(TAG, "write to '%s' failed, rolling back to %" PRIu32 " bytes", path.c_str(),
             state.openSegmentBytes);
    fclose(static_cast<FILE*>(state.openFile));
    state.openFile = nullptr;
    if (truncate(path.c_str(), static_cast<off_t>(state.openSegmentBytes)) != 0) {
        ESP_LOGW(TAG, "rollback truncate of '%s' failed", path.c_str());
    }
    return false;
}

void Journal::recoverOpenSegment(StreamState& state) {
    SegmentRecord& open = state.segments[state.segmentCount - 1];
    std::string path = segmentPath(state, open.segmentId);

    FrameScanner scanner;
    if (FILE* file = fopen(path.c_str(), "rb")) {
        uint8_t chunk[256];
        size_t got;
        while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) scanner.feed(chunk, got);
        fclose(file);
    }

    if (scanner.totalBytes() > scanner.validBytes()) {
        ESP_LOGW(TAG, "stream '%s': dropping %u bytes of a frame cut short in '%s'", state.filePrefix,
                 static_cast<unsigned>(scanner.totalBytes() - scanner.validBytes()), path.c_str());
        if (truncate(path.c_str(), static_cast<off_t>(scanner.validBytes())) != 0) {
            ESP_LOGW(TAG, "truncate of '%s' failed", path.c_str());
        }
    }

    uint32_t frames = scanner.completeFrames();
    uint32_t nextSeq = open.oldestSeq + frames;
    uint32_t validBytes = static_cast<uint32_t>(scanner.validBytes());
    if (nextSeq != state.cursor.nextSeq() || validBytes != state.openSegmentBytes) {
        ESP_LOGI(TAG, "stream '%s': rebuilt open segment from file: nextSeq %" PRIu32 " -> %" PRIu32
                      ", %" PRIu32 " bytes",
                 state.filePrefix, state.cursor.nextSeq(), nextSeq, validBytes);
    }

    open.newestSeq = frames > 0 ? nextSeq - 1 : open.oldestSeq;
    open.sizeBytes = validBytes;
    state.openSegmentBytes = validBytes;
    state.cursor.restore(nextSeq, state.cursor.oldestSeq(),
                         state.cursor.hasAck() ? std::optional<uint32_t>(state.cursor.ackedSeq()) : std::nullopt);
}

void Journal::appendToStream(StreamState& state, Stream stream, const LogRecord& record) {
    if (!state.mounted) return;

    // The seq is only consumed once the frame is on disk: readFrom() numbers
    // records by their position in the segment, so every assigned seq must
    // correspond to exactly one frame, or the seq embedded in later records
    // drifts away from the position Uplink reports it at.
    LogRecord stamped = record;
    uint32_t seq = state.cursor.nextSeq();
    stamped.seq = seq;

    std::vector<uint8_t> bytes = encodeLogRecord(stamped);
    if (!writeFrame(state, bytes)) return;
    state.cursor.assignSeq();

    uint32_t written = static_cast<uint32_t>(bytes.size() + 2);
    state.openSegmentBytes += written;
    SegmentRecord& open = state.segments[state.segmentCount - 1];
    open.newestSeq = seq;
    open.sizeBytes = state.openSegmentBytes;

    uint32_t dayIndex = uptimeDayIndex();
    writeAccounting_.addBytesWritten(stream, written, dayIndex);
    // Guarded two ways: `stream != Events` (routing a budget warning
    // through appendEvent() while already inside
    // appendToStream(eventsState_, ...) would otherwise be able to
    // re-trip its own check and recurse), and `lastBudgetWarnDay !=
    // dayIndex` (dailyBudgetExceeded() stays true for every write for
    // the rest of the day once tripped - without this, one exceeded
    // budget would emit one event per subsequent record, not one per
    // day).
    if (kMaxBytesPerDay > 0 && stream != Stream::Events && state.lastBudgetWarnDay != dayIndex &&
        writeAccounting_.dailyBudgetExceeded(stream, dayIndex, kMaxBytesPerDay)) {
        state.lastBudgetWarnDay = dayIndex;
        LogRecord budgetEvent{};
        budgetEvent.setTag("write_budget");
        char msg[64];
        std::snprintf(msg, sizeof(msg), "stream=%s exceeded=true hard=false", toString(stream));
        budgetEvent.setMessage(msg);
        budgetEvent.level = Level::Warn;
        // Kept as a plain record on this stream's own Events copy would
        // be circular (Data/Logs have none); route every budget warning
        // through the Events partition regardless of which stream
        // tripped it, same as any other cross-cutting health signal.
        appendEvent(budgetEvent);
    }

    rotateIfNeeded(state, stream);
}

void Journal::rotateIfNeeded(StreamState& state, Stream stream) {
    if (state.openSegmentBytes < kSegmentRotateBytes) return;

    if (state.openFile) {
        fclose(static_cast<FILE*>(state.openFile));
        state.openFile = nullptr;
    }
    state.segments[state.segmentCount - 1].closedAtUptimeSec =
        static_cast<uint32_t>(uptimeSeconds());

    if (state.segmentCount < kMaxTrackedSegments) {
        SegmentRecord next;
        next.segmentId = state.segments[state.segmentCount - 1].segmentId + 1;
        next.oldestSeq = state.cursor.nextSeq();
        next.newestSeq = state.cursor.nextSeq();
        state.segments[state.segmentCount] = next;
        ++state.segmentCount;
    } else {
        ESP_LOGW(TAG, "stream '%s' has too many untracked-for-reclaim segments; forcing reclaim",
                 toString(stream));
    }
    state.openSegmentBytes = 0;

    saveIndex(state);

    // Events segments are never handed to the reclaimer (plan §8.8 step
    // 5: "nunca hasta el final: incidentes, eventos..."). Rotation still
    // closes and tracks them exactly like Logs/Data, but nothing ever
    // deletes one - the "journal" partition's own free-space pressure is
    // instead relieved by reclaiming Logs segments (see maybeReclaim()).
    if (stream == Stream::Events) return;

    reclaimMount(state, state.partitionLabel,
                 stream == Stream::Data ? kDataRetentionSeconds : UINT64_MAX);
}

void Journal::reclaimMount(StreamState& state, const char* partitionLabel,
                            uint64_t retentionSeconds) {
    size_t total = 0, used = 0;
    if (esp_littlefs_info(partitionLabel, &total, &used) != ESP_OK || total == 0) return;
    size_t free = total > used ? total - used : 0;

    std::vector<SegmentInfo> candidates;
    uint64_t now = uptimeSeconds();
    Stream ownerStream = &state == &dataState_ ? Stream::Data
                          : &state == &eventsState_ ? Stream::Events
                                                     : Stream::Logs;
    for (uint32_t i = 0; i + 1 < state.segmentCount; ++i) {  // exclude the open (last) segment
        const SegmentRecord& segment = state.segments[i];
        bool acked = state.cursor.hasAck() && segment.newestSeq <= state.cursor.ackedSeq();
        uint64_t age = now > segment.closedAtUptimeSec ? now - segment.closedAtUptimeSec : 0;
        candidates.push_back(
            {ownerStream, segment.segmentId, segment.oldestSeq, segment.newestSeq,
             segment.sizeBytes, acked, age});
    }

    auto result = SpaceReclaimer::reclaim(candidates, total, free, retentionSeconds,
                                           kLowWaterMarkPercent, kCriticalMarkPercent);

    for (uint32_t deletedId : result.segmentIdsToDelete) {
        for (uint32_t i = 0; i < state.segmentCount; ++i) {
            if (state.segments[i].segmentId != deletedId) continue;

            std::remove(segmentPath(state, deletedId).c_str());

            auto gap = state.cursor.reclaimTo(state.segments[i].newestSeq + 1, "space_pressure");
            if (gap) {
                LogRecord gapEvent{};
                gapEvent.setTag("log_gap");
                gapEvent.level = Level::Warn;
                char msg[96];
                std::snprintf(msg, sizeof(msg), "stream=%s from=%" PRIu32 " to=%" PRIu32 " count=%" PRIu32,
                              toString(gap->stream), gap->fromSeq, gap->toSeq, gap->count);
                gapEvent.setMessage(msg);
                appendEvent(gapEvent);
            }

            for (uint32_t j = i; j + 1 < state.segmentCount; ++j) state.segments[j] = state.segments[j + 1];
            --state.segmentCount;
            break;
        }
    }

    if (result.degradedToRamOnly) {
        LogRecord critical{};
        critical.setTag("storage_critical");
        critical.level = Level::Error;
        char msg[64];
        std::snprintf(msg, sizeof(msg), "partition=%s reclaim_insufficient=true", partitionLabel);
        critical.setMessage(msg);
        appendEvent(critical);
    }

    if (!result.segmentIdsToDelete.empty()) saveIndex(state);
}

void Journal::appendLog(const LogRecord& record) { appendToStream(logsState_, Stream::Logs, record); }

void Journal::appendEvent(const LogRecord& record) {
    appendToStream(eventsState_, Stream::Events, record);
}

std::optional<uint32_t> Journal::appendRaw(Stream stream, const uint8_t* data, size_t len) {
    StreamState& state = stateFor(stream);
    if (!state.mounted) return std::nullopt;

    // Same rule as appendToStream(): no seq without a frame on disk.
    std::vector<uint8_t> bytes(data, data + len);
    if (!writeFrame(state, bytes)) return std::nullopt;
    uint32_t seq = state.cursor.assignSeq();

    uint32_t written = static_cast<uint32_t>(bytes.size() + 2);
    state.openSegmentBytes += written;
    SegmentRecord& open = state.segments[state.segmentCount - 1];
    open.newestSeq = seq;
    open.sizeBytes = state.openSegmentBytes;

    writeAccounting_.addBytesWritten(stream, written, uptimeDayIndex());

    rotateIfNeeded(state, stream);
    return seq;
}

void Journal::maybeReclaim() {
    if (logsState_.mounted) reclaimMount(logsState_, logsState_.partitionLabel, UINT64_MAX);
    if (dataState_.mounted) reclaimMount(dataState_, dataState_.partitionLabel, kDataRetentionSeconds);
    // Events is deliberately never passed to reclaimMount() from here:
    // incidents/events are never sacrificed (plan §8.8 step 5) - the only
    // reason reclaimMount() takes a StreamState at all for Events
    // internally (via appendEvent's own rotation path) is bookkeeping,
    // not space pressure response.
}

Journal::StreamState& Journal::stateFor(Stream stream) {
    return stream == Stream::Data ? dataState_ : stream == Stream::Events ? eventsState_ : logsState_;
}

std::string Journal::segmentPath(const StreamState& state, uint32_t segmentId) {
    char name[64];
    std::snprintf(name, sizeof(name), "%s/%s_%05" PRIu32 ".cbor", state.mountPath, state.filePrefix,
                  segmentId);
    return name;
}

void Journal::ack(Stream stream, uint32_t upToSeqInclusive) {
    StreamState& state = stateFor(stream);
    state.cursor.ack(upToSeqInclusive);
    saveIndex(state);
}

bool Journal::streamAvailable(Stream stream) const {
    return const_cast<Journal*>(this)->stateFor(stream).mounted;
}

uint32_t Journal::nextSeq(Stream stream) const {
    return const_cast<Journal*>(this)->stateFor(stream).cursor.nextSeq();
}

uint64_t Journal::lifetimeBytesWritten(Stream stream) const {
    return writeAccounting_.lifetimeBytes(stream);
}

uint32_t Journal::firstUnackedSeq(Stream stream) const {
    const JournalCursor& cursor = const_cast<Journal*>(this)->stateFor(stream).cursor;
    return cursor.hasAck() ? cursor.ackedSeq() + 1 : 0;
}

std::optional<Journal::PartitionUsage> Journal::partitionUsage(Stream stream) const {
    StreamState& state = const_cast<Journal*>(this)->stateFor(stream);
    if (!state.mounted) return std::nullopt;
    size_t total = 0, used = 0;
    if (esp_littlefs_info(state.partitionLabel, &total, &used) != ESP_OK) return std::nullopt;
    return PartitionUsage{total, used};
}

uint64_t Journal::partitionLifetimeBytes(const char* partitionLabel) const {
    uint64_t sum = 0;
    for (Stream s : {Stream::Logs, Stream::Events, Stream::Data}) {
        if (std::strcmp(const_cast<Journal*>(this)->stateFor(s).partitionLabel, partitionLabel) == 0) {
            sum += writeAccounting_.lifetimeBytes(s);
        }
    }
    return sum;
}

double Journal::estimatedWearPercent(Stream stream, uint32_t enduranceCycles) const {
    auto usage = partitionUsage(stream);
    if (!usage) return -1.0;
    const char* label = const_cast<Journal*>(this)->stateFor(stream).partitionLabel;
    return writeAccounting_.estimatedWearPercent(partitionLifetimeBytes(label), usage->totalBytes,
                                                  enduranceCycles);
}

std::optional<Journal::RawBatch> Journal::readFrom(Stream stream, uint32_t fromSeqInclusive,
                                                     size_t maxBytes, size_t maxRecords) {
    StreamState& state = stateFor(stream);
    if (!state.mounted || maxRecords == 0) return std::nullopt;

    RawBatch batch;
    bool started = false;
    uint32_t nextWantedSeq = fromSeqInclusive;

    for (uint32_t i = 0; i < state.segmentCount; ++i) {
        const SegmentRecord& segment = state.segments[i];
        if (segment.sizeBytes == 0) continue;             // nothing written here yet
        if (segment.newestSeq < nextWantedSeq) continue;  // entirely already consumed/before what we want

        // If this is the currently-open-for-append segment, its on-disk
        // directory entry (size/CTZ pointers) in LittleFS is only synced
        // on close - fflush() after each write (see writeLengthPrefixed())
        // only flushes the C stdio buffer into LittleFS's own write
        // buffer, not that metadata commit. A second, independent fopen()
        // on the same path while the append handle is still open therefore
        // sees a STALE size (0 bytes, from before this boot's writes) and
        // silently reads nothing - no error, just an empty file - which is
        // indistinguishable from "nothing written yet" without instrumentation.
        // Closing the write handle here forces that sync; the next append
        // call lazily reopens it (appendToStream()/appendRaw() both check
        // `!state.openFile` and reopen in "ab", which continues safely at
        // EOF - no data loss).
        if (state.openFile && i + 1 == state.segmentCount) {
            fclose(static_cast<FILE*>(state.openFile));
            state.openFile = nullptr;
        }

        std::string path = segmentPath(state, segment.segmentId);
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) {
            ESP_LOGW(TAG, "could not open segment '%s' for reading", path.c_str());
            break;  // stop rather than silently skip a gap in the middle of a batch
        }

        // Read this segment in small, bounded chunks instead of
        // `fread`-ing the whole (up to kSegmentRotateBytes, 64 KB) file
        // into one buffer up front: on a heap already under pressure
        // from WiFi/TLS/MQTT, that up-front read's own vector growth
        // could (and on device, did - see documentation/iot.md's trap
        // write-up) fail a reallocation and abort() the whole firmware,
        // since this build has C++ exceptions off (decisions/0001) and
        // can't catch std::bad_alloc. `pending` only ever holds an
        // in-progress (not-yet-complete) frame plus one fresh chunk -
        // accumulateFrames() eats everything complete out of it on
        // every iteration - so its size never scales with the
        // segment's total size, only with `kReadChunkBytes` and the
        // largest possible single frame (bounded by FrameCodec's
        // 16-bit length prefix, 64 KiB).
        // 512, not something larger: this runs on the `croniot_uplink`
        // task, whose whole stack is only
        // CONFIG_CRONIOT_UPLINK_TASK_STACK_SIZE bytes (4096 by default -
        // see telemetry/Uplink.cpp) - a bigger on-stack chunk buffer
        // here was tried and immediately stack-overflowed that task
        // ("Guru Meditation ... Stack protection fault") on real
        // hardware. 512 matches what the original (pre-chunking)
        // version of this function already used safely for its own
        // `fread` buffer.
        constexpr size_t kReadChunkBytes = 512;
        std::vector<uint8_t> pending;
        uint8_t chunk[kReadChunkBytes];
        uint32_t seq = segment.oldestSeq;
        bool limitReached = false;
        size_t got;
        do {
            got = fread(chunk, 1, sizeof(chunk), file);
            if (got > 0) pending.insert(pending.end(), chunk, chunk + got);

            // `segment.oldestSeq` is the seq of the first record
            // physically in this file (set when the segment was
            // created - see rotateIfNeeded()/loadIndex()). Walking
            // frames from byte 0 and skipping any still below
            // `nextWantedSeq` also naturally implements the "clamp up
            // if reclamation already deleted earlier data" behavior
            // documented on readFrom() in Journal.h: if
            // `nextWantedSeq` is below `segment.oldestSeq`, `seq`
            // starts already at-or-above it, so nothing gets skipped.
            //
            // Wire format (BatchEnvelope.h): `records[]` is a CBOR
            // array whose elements are these bytes back-to-back, so
            // only the *payload* FrameCodec isolates (what
            // accumulateFrames() appends) belongs in `batch.frames` -
            // the on-disk 2-byte length prefix is a Journal-internal
            // framing detail, not part of the record's CBOR encoding.
            auto result = accumulateFrames(pending.data(), pending.size(), seq, nextWantedSeq, started,
                                            batch.firstSeq, batch.frames, batch.count, maxBytes, maxRecords);
            if (result.consumedBytes > 0) {
                pending.erase(pending.begin(), pending.begin() + static_cast<long>(result.consumedBytes));
            }
            if (result.limitReached) {
                limitReached = true;
                break;
            }
        } while (got == sizeof(chunk));  // short read (incl. 0) == this fread hit EOF
        fclose(file);
        nextWantedSeq = seq;

        if (limitReached || batch.count >= maxRecords || batch.frames.size() >= maxBytes) break;
    }

    if (!started) return std::nullopt;
    return batch;
}

}  // namespace croniot::log
