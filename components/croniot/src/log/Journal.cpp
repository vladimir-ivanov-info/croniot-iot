#include "Journal.h"

#include <cstdio>
#include <cstring>

#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "CborWriter.h"
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
void writeLengthPrefixed(FILE* file, const std::vector<uint8_t>& bytes) {
    uint16_t len = static_cast<uint16_t>(bytes.size());
    uint8_t header[2] = {static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>(len >> 8)};
    fwrite(header, 1, sizeof(header), file);
    fwrite(bytes.data(), 1, bytes.size(), file);
    fflush(file);
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

void Journal::appendToStream(StreamState& state, Stream stream, const LogRecord& record) {
    if (!state.mounted) return;

    LogRecord stamped = record;
    uint32_t seq = state.cursor.assignSeq();
    stamped.seq = seq;

    std::vector<uint8_t> bytes = encodeLogRecord(stamped);

    if (!state.openFile) {
        uint32_t openId = state.segments[state.segmentCount - 1].segmentId;
        char name[64];
        std::snprintf(name, sizeof(name), "%s/%s_%05u.cbor", state.mountPath, state.filePrefix,
                      openId);
        state.openFile = fopen(name, "ab");
        if (!state.openFile) {
            ESP_LOGW(TAG, "could not open segment '%s'", name);
            return;
        }
    }

    writeLengthPrefixed(static_cast<FILE*>(state.openFile), bytes);

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

            char name[64];
            std::snprintf(name, sizeof(name), "%s/%s_%05u.cbor", state.mountPath, state.filePrefix,
                          deletedId);
            std::remove(name);

            auto gap = state.cursor.reclaimTo(state.segments[i].newestSeq + 1, "space_pressure");
            if (gap) {
                LogRecord gapEvent{};
                gapEvent.setTag("log_gap");
                gapEvent.level = Level::Warn;
                char msg[96];
                std::snprintf(msg, sizeof(msg), "stream=%s from=%u to=%u count=%u",
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

void Journal::maybeReclaim() {
    if (logsState_.mounted) reclaimMount(logsState_, logsState_.partitionLabel, UINT64_MAX);
    if (dataState_.mounted) reclaimMount(dataState_, dataState_.partitionLabel, kDataRetentionSeconds);
    // Events is deliberately never passed to reclaimMount() from here:
    // incidents/events are never sacrificed (plan §8.8 step 5) - the only
    // reason reclaimMount() takes a StreamState at all for Events
    // internally (via appendEvent's own rotation path) is bookkeeping,
    // not space pressure response.
}

void Journal::ack(Stream stream, uint32_t upToSeqInclusive) {
    StreamState& state = stream == Stream::Data ? dataState_
                          : stream == Stream::Events ? eventsState_
                                                       : logsState_;
    state.cursor.ack(upToSeqInclusive);
    saveIndex(state);
}

bool Journal::streamAvailable(Stream stream) const {
    const StreamState& state = stream == Stream::Data ? dataState_
                                : stream == Stream::Events ? eventsState_
                                                             : logsState_;
    return state.mounted;
}

uint32_t Journal::nextSeq(Stream stream) const {
    const StreamState& state = stream == Stream::Data ? dataState_
                                : stream == Stream::Events ? eventsState_
                                                             : logsState_;
    return state.cursor.nextSeq();
}

uint64_t Journal::lifetimeBytesWritten(Stream stream) const {
    return writeAccounting_.lifetimeBytes(stream);
}

}  // namespace croniot::log
