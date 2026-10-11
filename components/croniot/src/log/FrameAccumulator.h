#ifndef CRONIOT_LOG_FRAMEACCUMULATOR_H
#define CRONIOT_LOG_FRAMEACCUMULATOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace croniot::log {

// Pure, file-I/O-free core of Journal::readFrom()'s frame-accumulation
// step, pulled out specifically so it is unit-testable on a plain
// in-memory buffer - Journal itself needs a real esp_vfs_littlefs mount
// to exercise end to end (no fake exists for that here, see
// test_host/README), but the "given whatever bytes happen to be in
// memory right now, walk complete frames and decide whether to keep
// going" logic underneath it has no such dependency.
//
// Why this exists at all (see documentation/iot.md's trap write-up for
// the incident this fixes): the original readFrom() `fread`'d an entire
// LittleFS segment (up to kSegmentRotateBytes, 64 KB) into one
// std::vector *before* ever looking at maxBytes/maxRecords - on a
// heap already under pressure from WiFi/TLS/MQTT, the vector's grow
// reallocations (512 -> 1024 -> ... -> 32768 -> 65536) could fail
// outright, and since this firmware builds with C++ exceptions off
// (see decisions/0001), operator new's std::bad_alloc became an
// abort() instead of something readFrom() could handle. The caller
// (Journal::readFrom(), now chunked - see Journal.cpp) instead feeds
// this function one small fread() chunk at a time and stops asking for
// more as soon as a limit is hit, so the only allocation whose size
// scales with caller-chosen `maxBytes` is `frames` itself - never a
// whole segment's worth regardless of how small the actual batch asked
// for is.
struct AccumulateResult {
    // How many bytes at the front of `buf` were fully consumed (i.e.
    // belong to complete frames this call has already decided about -
    // either skipped for being `< nextWantedSeq`, or appended to
    // `frames`). The caller should erase buf[0, consumedBytes) before
    // appending its next chunk and calling again; the trailing,
    // unconsumed bytes are an in-progress frame that needs more data.
    size_t consumedBytes = 0;

    // True once `count >= maxRecords` or `frames.size() >= maxBytes`:
    // the caller must stop feeding this segment (and the whole
    // readFrom() call) more data, full stop - a batch limit was
    // reached, not end of buffer.
    bool limitReached = false;
};

// Walks every complete frame in `buf[0, len)` (via FrameCodec), in
// strict seq order starting at `seq` (the caller's running cursor into
// the segment, incremented here once per frame whether skipped or
// kept - see Journal.h's "known scoped gap" note on why seq is never
// re-derived from on-disk bytes), until either:
//   - the buffer runs out of bytes for another full frame (ordinary
//     "need another fread() chunk" exit - `limitReached` stays false),
//     or
//   - `maxBytes`/`maxRecords` is reached (`limitReached` becomes true).
//
// Frames with `seq < nextWantedSeq` are skipped (already consumed by
// an earlier call/batch) without touching `frames`/`count`/`started`/
// `firstSeq`. The first frame actually kept sets `firstSeq` (only if
// `started` was false coming in) and flips `started` to true - mirrors
// Journal::RawBatch's own fields 1:1 so Journal.cpp can pass them
// straight through by reference.
AccumulateResult accumulateFrames(const uint8_t* buf, size_t len, uint32_t& seq, uint32_t nextWantedSeq,
                                   bool& started, uint32_t& firstSeq, std::vector<uint8_t>& frames,
                                   uint32_t& count, size_t maxBytes, size_t maxRecords);

}  // namespace croniot::log

#endif
