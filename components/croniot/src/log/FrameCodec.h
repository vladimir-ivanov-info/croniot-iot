#ifndef CRONIOT_LOG_FRAMECODEC_H
#define CRONIOT_LOG_FRAMECODEC_H

#include <cstddef>
#include <cstdint>

namespace croniot::log {

// Walks the on-disk frame format Journal.cpp writes: a 2-byte
// little-endian length prefix followed by that many bytes of CBOR (see
// Journal.cpp's writeLengthPrefixed()). Pure buffer math, no file I/O -
// Journal.cpp's segment reader (added for the uplink's batch-building,
// see telemetry/Uplink.cpp) calls this on bytes it already `fread` in;
// tests drive it directly on an in-memory buffer.
class FrameCodec {
public:
    // Reads the frame starting at `buf[offset]`. On success, sets
    // `frameStart`/`frameLen` to the *payload* (CBOR bytes, prefix
    // excluded) and advances `offset` past the whole frame (prefix +
    // payload) so a caller can call this again to get the next one.
    // Returns false - and leaves `offset` untouched - if fewer than a
    // full frame remains, e.g. a length prefix with no payload after it
    // yet (a write that was cut off mid-frame by a crash, or simply the
    // reader catching up to where the writer currently is).
    static bool readNextFrame(const uint8_t* buf, size_t len, size_t& offset, size_t& frameStart,
                               size_t& frameLen) {
        if (offset + 2 > len) return false;
        uint16_t payloadLen = static_cast<uint16_t>(buf[offset]) |
                               (static_cast<uint16_t>(buf[offset + 1]) << 8);
        if (offset + 2 + payloadLen > len) return false;

        frameStart = offset + 2;
        frameLen = payloadLen;
        offset += 2 + payloadLen;
        return true;
    }

    // Counts complete frames in `buf[0, len)` without needing to know
    // anything about their contents - used by Journal's reader to turn a
    // byte position into a record count (and vice versa), since records
    // are written in strict, gap-free seq order within one segment (see
    // Journal.h's "known scoped gap" note on why seq itself is never
    // re-derived from the bytes).
    static size_t countFrames(const uint8_t* buf, size_t len) {
        size_t offset = 0, count = 0, start = 0, frameLen = 0;
        while (readNextFrame(buf, len, offset, start, frameLen)) ++count;
        return count;
    }
};

// Streaming counterpart to countFrames(): fed a segment file in chunks of
// any size, counts its complete frames and where the last one ends, without
// ever holding more than one chunk. Journal uses it at boot to rebuild the
// open segment's cursor from the file itself (see
// Journal::recoverOpenSegment()); a trailing frame cut short by a power loss
// shows up as totalBytes() > validBytes().
class FrameScanner {
public:
    void feed(const uint8_t* data, size_t len) {
        size_t i = 0;
        while (i < len) {
            if (payloadRemaining_ > 0) {
                size_t take = payloadRemaining_ < len - i ? payloadRemaining_ : len - i;
                i += take;
                totalBytes_ += take;
                payloadRemaining_ -= take;
                if (payloadRemaining_ == 0) completeFrame();
                continue;
            }
            prefix_[prefixHave_++] = data[i++];
            ++totalBytes_;
            if (prefixHave_ == 2) {
                prefixHave_ = 0;
                payloadRemaining_ = static_cast<size_t>(prefix_[0]) | (static_cast<size_t>(prefix_[1]) << 8);
                if (payloadRemaining_ == 0) completeFrame();
            }
        }
    }

    uint32_t completeFrames() const { return completeFrames_; }
    size_t validBytes() const { return validBytes_; }
    size_t totalBytes() const { return totalBytes_; }

private:
    void completeFrame() {
        ++completeFrames_;
        validBytes_ = totalBytes_;
    }

    uint8_t prefix_[2] = {0, 0};
    uint8_t prefixHave_ = 0;
    size_t payloadRemaining_ = 0;
    uint32_t completeFrames_ = 0;
    size_t validBytes_ = 0;
    size_t totalBytes_ = 0;
};

}  // namespace croniot::log

#endif
