#ifndef CRONIOT_TELEMETRY_UPLINK_H
#define CRONIOT_TELEMETRY_UPLINK_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "log/JournalTypes.h"

namespace croniot::telemetry {

// Drains Journal's Logs/Events streams to the server (plan §5/§12.4):
// picks the next stream via UplinkScheduler, reads one batch via
// Journal::readFrom(), sends it, and retries with RetryPolicy's backoff
// until either an application-level ack arrives (via MessageBus's ack
// subscription -> Journal::ack(), which is what actually advances the
// cursor - never the MQTT PUBACK) or the attempt limit gives up on that
// *attempt* (the data itself stays in the Journal regardless - nothing
// here ever drops a record on give-up, only stops retrying it for now).
//
// Known scoped simplification: the plan's transport table (§4) puts
// "immediate" events on their own topic/latency class distinct from
// batched logs. This drains both Logs and Events through the exact same
// cursor/ack/retry machinery, one batch at a time, and gets "events
// first" purely from UplinkScheduler's priority order rather than a
// second, parallel fire-and-forget path - simpler, and avoids splitting
// the one dedup/ack contract in two. Stream::Data drains through this
// exact same machinery too (plan §7.2/§12.6) - its wire topic
// (`/iot_to_server/sensor_batch/<uuid>`) is real now, but nothing on
// this path ever decodes a Data frame's payload; it's opaque bytes to
// Uplink either way, same as Logs/Events records are.
class Uplink {
public:
    static Uplink& instance();

    // Starts the drain task (idempotent) and subscribes to the ack/
    // log_config topics through MessageBus. Call once MessageBus has a
    // channel that's actually connected - CommonSetup::setup(), NOT
    // Log::init() (which runs before any network exists at all; see
    // plan §10's "CommonSetup::setup() solo engancha el uplink cuando
    // MessageBus está listo").
    void start();

    // Wired to MessageBus::subscribeAck()'s callback. Expects
    // {"stream":"logs"|"events"|"data","upToSeq":<uint>} (plan §5 point
    // 3's ack shape, minus `bootId`/`batchId` - see Uplink.cpp for why
    // this Journal doesn't need either to stay correct).
    void onAck(const std::string& json);

    // Current resend-attempt count for whichever batch is in flight on
    // `stream`, 0 if none - a health-report signal (plan §5 point 13's
    // "resends"), not used by the drain loop itself.
    uint32_t currentAttempt(croniot::log::Stream stream) const;

private:
    Uplink() = default;

    static void run(void* arg);
    void serviceOnce();
    bool sendBatch(croniot::log::Stream stream, uint32_t firstSeq, uint32_t count,
                   const std::vector<uint8_t>& frames);

    struct InFlightBatch {
        bool active = false;
        uint32_t firstSeq = 0;
        uint32_t count = 0;
        uint32_t attempt = 0;
        uint64_t nextSendDueMs = 0;
        std::vector<uint8_t> frames;  // resend verbatim on retry - same batch, same bytes
    };

    std::array<InFlightBatch, 3> inFlight_{};
    // Wall-clock (uptime ms) each stream's backlog last went from empty
    // to non-empty, 0 while empty - lets serviceOnce() compute a real
    // "how long has this stream been waiting" instead of confusing it
    // with process uptime (see Uplink.cpp).
    std::array<uint64_t, 3> pendingSinceMs_{};
    bool started_ = false;
};

}  // namespace croniot::telemetry

#endif
