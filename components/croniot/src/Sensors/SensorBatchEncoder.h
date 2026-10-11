#ifndef CRONIOT_SENSORS_SENSORBATCHENCODER_H
#define CRONIOT_SENSORS_SENSORBATCHENCODER_H

#include <cstdint>
#include <vector>

#include "SensorReportBuffer.h"
#include "log/CborWriter.h"

namespace croniot {

// Wire shape: [sensorUid, t0Ms, dtMs, values[]] (plan §7.2, simplified).
// Deliberately does NOT carry seq/bootId/count the way the plan's
// original per-batch header sketch (§3.1/§7.2) does - those are already
// carried by the *outer* Uplink batch envelope this payload gets
// wrapped in verbatim (see BatchEnvelope.h: `[bootId, stream, firstSeq,
// count, records[]]`), the exact same generic frame/envelope pipeline
// logs and events already use via Journal. Reusing it here rather than
// inventing a third sequencing scheme is the whole point - Uplink's
// scheduler, retry/ack machinery, and Journal's space reclamation don't
// need to know or care that a Data-stream frame's payload happens to be
// sensor readings instead of a log line.
//
// `dtMs` is the sensor's own fixed samplePeriodMs, constant because
// sampling is periodic by design (plan §7.2) - not present per-sample,
// only once per batch, since it never varies within one.
inline std::vector<uint8_t> encodeSensorBatch(int sensorUid, uint64_t t0Ms, uint64_t dtMs,
                                                const std::vector<SensorSample>& samples) {
    croniot::log::CborWriter writer;
    writer.writeArrayHeader(4);
    writer.writeUnsigned(static_cast<uint64_t>(sensorUid));
    writer.writeUnsigned(t0Ms);
    writer.writeUnsigned(dtMs);
    writer.writeArrayHeader(samples.size());
    for (const auto& sample : samples) writer.writeDouble(sample.value);
    return writer.bytes();
}

}  // namespace croniot

#endif
