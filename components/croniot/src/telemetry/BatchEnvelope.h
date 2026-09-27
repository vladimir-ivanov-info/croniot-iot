#ifndef CRONIOT_TELEMETRY_BATCHENVELOPE_H
#define CRONIOT_TELEMETRY_BATCHENVELOPE_H

#include <cstdint>
#include <vector>

#include "log/CborWriter.h"
#include "log/JournalTypes.h"

namespace croniot::telemetry {

// Wire shape for one uplink batch (plan §5: "un lote es {stream, bootId,
// firstSeq, count, records[]}"), as a 5-element CBOR array:
// [bootId, stream, firstSeq, count, records].
//
// `rawRecords` is the exact concatenation of `count` already-CBOR-encoded
// LogRecord values, byte-for-byte what Journal wrote to its segment files
// (see log/FrameCodec.h and Journal::readFrom()) - CBOR's own definite-
// length-array encoding is just an array header followed by that many
// complete values back-to-back, so appending those bytes verbatim after
// writing the array header IS a valid CBOR array. Nothing here ever
// decodes a record to re-encode it (plan §3.1: "es el mismo byte-stream...
// no se recodifica").
inline std::vector<uint8_t> encodeBatchEnvelope(uint32_t bootId, croniot::log::Stream stream,
                                                 uint32_t firstSeq, uint32_t count,
                                                 const std::vector<uint8_t>& rawRecords) {
    croniot::log::CborWriter writer;
    writer.writeArrayHeader(5);
    writer.writeUnsigned(bootId);
    writer.writeUnsigned(static_cast<uint64_t>(stream));
    writer.writeUnsigned(firstSeq);
    writer.writeUnsigned(count);
    writer.writeArrayHeader(count);

    std::vector<uint8_t> bytes = writer.bytes();
    bytes.insert(bytes.end(), rawRecords.begin(), rawRecords.end());
    return bytes;
}

}  // namespace croniot::telemetry

#endif
