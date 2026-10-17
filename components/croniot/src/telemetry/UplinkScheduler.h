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
    bool retryDue = false;   // inFlight, and its ack timeout/backoff window has expired
};

// Decides which single stream gets the next batch (plan §5 point 9:
// "events > logs > data, con cuota mínima para data para que no se quede
// sin turno"). Only one in-flight batch per stream at a time (plan §5
// point 4: "un lote en vuelo por stream") - a stream already `inFlight`
// is only picked again once its retry is due (`retryDue`), so the caller
// can resend that same batch or give up on it. Without that exception a
// lost or mismatched ack left the stream in flight forever: the caller's
// retry/give-up branch was never reached and the stream stopped draining
// until a reboot (seen on device: Logs silent for hours while Data flowed).
class UplinkScheduler {
public:
    // `dataStarvationMs`/`logsStarvationMs`: if Stream::Data/Stream::Logs
    // has been waiting at least this long while a higher-priority stream
    // would otherwise keep winning, it's picked instead - the
    // anti-starvation quota, same idea applied one priority level up.
    // Without this, a device producing Events continuously (each ack
    // just making room for the next one, so Events is rarely `inFlight`
    // long enough for Logs's turn) could starve Logs indefinitely even
    // though neither stream is ever stuck - `inFlight` alone only
    // protects a stream *currently* being sent, not one that's simply
    // always out-prioritized by a never-empty higher stream. Returns
    // nullopt if nothing is eligible (every stream is empty, or in flight
    // with its retry not yet due).
    static std::optional<croniot::log::Stream> pickNext(const std::vector<StreamBacklog>& backlogs,
                                                          uint64_t dataStarvationMs,
                                                          uint64_t logsStarvationMs = UINT64_MAX) {
        // A due retry goes first: it is a batch already on the wire, and
        // the resend (or the give-up that frees the stream) can't wait on
        // fresh batches from the other streams.
        for (auto stream : {croniot::log::Stream::Events, croniot::log::Stream::Logs,
                             croniot::log::Stream::Data}) {
            const StreamBacklog* backlog = find(backlogs, stream);
            if (backlog && backlog->inFlight && backlog->retryDue) return stream;
        }

        const StreamBacklog* data = find(backlogs, croniot::log::Stream::Data);
        if (data && data->hasPending && !data->inFlight && data->oldestPendingAgeMs >= dataStarvationMs) {
            return croniot::log::Stream::Data;
        }

        const StreamBacklog* logs = find(backlogs, croniot::log::Stream::Logs);
        if (logs && logs->hasPending && !logs->inFlight && logs->oldestPendingAgeMs >= logsStarvationMs) {
            return croniot::log::Stream::Logs;
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
