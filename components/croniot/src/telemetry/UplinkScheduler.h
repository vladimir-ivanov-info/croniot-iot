#ifndef CRONIOT_TELEMETRY_UPLINKSCHEDULER_H
#define CRONIOT_TELEMETRY_UPLINKSCHEDULER_H

#include <cstdint>
#include <optional>
#include <vector>

#include "log/JournalTypes.h"

namespace croniot::telemetry {

// One stream's uplink state, as the scheduler needs to see it - not the
// Journal's own bookkeeping, just enough to decide whose turn is next.
struct StreamBacklog {
    croniot::log::Stream stream;
    bool hasPending;         // there is at least one unacked record to send
    bool inFlight;           // a batch for this stream is already awaiting ack/retry
    uint64_t oldestPendingAgeMs;  // 0 if !hasPending
};

// Decides which single stream gets the next batch (plan §5 point 9:
// "events > logs > data, con cuota mínima para data para que no se quede
// sin turno"). Only one in-flight batch per stream at a time (plan §5
// point 4: "un lote en vuelo por stream") - a stream already `inFlight`
// is never picked again until that resolves (ack or give-up).
class UplinkScheduler {
public:
    // `dataStarvationMs`: if Stream::Data has been waiting at least this
    // long while a higher-priority stream would otherwise keep winning,
    // Data is picked instead - the anti-starvation quota. Returns nullopt
    // if nothing is eligible (every stream is empty or already in flight).
    static std::optional<croniot::log::Stream> pickNext(const std::vector<StreamBacklog>& backlogs,
                                                          uint64_t dataStarvationMs) {
        const StreamBacklog* data = find(backlogs, croniot::log::Stream::Data);
        if (data && data->hasPending && !data->inFlight && data->oldestPendingAgeMs >= dataStarvationMs) {
            return croniot::log::Stream::Data;
        }

        for (auto stream : {croniot::log::Stream::Events, croniot::log::Stream::Logs,
                             croniot::log::Stream::Data}) {
            const StreamBacklog* backlog = find(backlogs, stream);
            if (backlog && backlog->hasPending && !backlog->inFlight) return stream;
        }
        return std::nullopt;
    }

private:
    static const StreamBacklog* find(const std::vector<StreamBacklog>& backlogs,
                                      croniot::log::Stream stream) {
        for (const auto& backlog : backlogs) {
            if (backlog.stream == stream) return &backlog;
        }
        return nullptr;
    }
};

}  // namespace croniot::telemetry

#endif
