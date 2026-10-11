#include "FrameAccumulator.h"

#include "FrameCodec.h"

namespace croniot::log {

AccumulateResult accumulateFrames(const uint8_t* buf, size_t len, uint32_t& seq, uint32_t nextWantedSeq,
                                   bool& started, uint32_t& firstSeq, std::vector<uint8_t>& frames,
                                   uint32_t& count, size_t maxBytes, size_t maxRecords) {
    AccumulateResult result;
    size_t offset = 0, frameStart = 0, frameLen = 0;

    while (FrameCodec::readNextFrame(buf, len, offset, frameStart, frameLen)) {
        if (seq < nextWantedSeq) {
            ++seq;
            result.consumedBytes = offset;
            continue;
        }

        if (count >= maxRecords || frames.size() >= maxBytes) {
            // Don't consume this frame - it wasn't actually accounted
            // for, so leave `offset` (and thus `consumedBytes`) exactly
            // where it was before this iteration started.
            result.limitReached = true;
            break;
        }

        if (!started) {
            firstSeq = seq;
            started = true;
        }
        frames.insert(frames.end(), buf + frameStart, buf + frameStart + frameLen);
        ++count;
        ++seq;
        result.consumedBytes = offset;
    }

    return result;
}

}  // namespace croniot::log
