#ifndef CRONIOT_TELEMETRY_RETRYPOLICY_H
#define CRONIOT_TELEMETRY_RETRYPOLICY_H

#include <cstdint>

namespace croniot::telemetry {

// Pure backoff/give-up math for a single in-flight batch (plan §5 point
// 5: "si no llega ack en 30s, se reenvía el mismo lote con el mismo
// batchId, con backoff exponencial y jitter. Tras K intentos se marca el
// enlace como degradado"). No clock, no RNG: the caller supplies both
// (`attempt` from its own retry counter, `jitterFraction` from whatever
// random source it likes) so this stays deterministic and testable, same
// pattern as LevelResolver's explicit `nowMs`.
class RetryPolicy {
public:
    // `attempt` is how many sends have already happened for this batch (1
    // after the first send). Delay doubles per attempt, capped at
    // `capDelayMs`, then jittered upward by up to `jitterFraction` (0.0-1.0)
    // of that capped delay - never downward, so a retry is never sent
    // *earlier* than the base schedule would allow.
    static uint64_t nextDelayMs(uint32_t attempt, uint64_t baseDelayMs, uint64_t capDelayMs,
                                 double jitterFraction) {
        uint64_t delay = baseDelayMs;
        for (uint32_t i = 1; i < attempt && delay < capDelayMs; ++i) {
            delay = delay > capDelayMs / 2 ? capDelayMs : delay * 2;
        }
        if (delay > capDelayMs) delay = capDelayMs;
        if (jitterFraction < 0.0) jitterFraction = 0.0;
        if (jitterFraction > 1.0) jitterFraction = 1.0;
        return delay + static_cast<uint64_t>(static_cast<double>(delay) * jitterFraction);
    }

    // True once `attempt` reaches `maxAttempts` - the caller should stop
    // retrying this batch and mark the link degraded (plan: "se deja de
    // drenar" - draining stops, but the data stays in the Journal; this
    // is a delivery-attempt limit, not a data-loss decision).
    static bool shouldGiveUp(uint32_t attempt, uint32_t maxAttempts) { return attempt >= maxAttempts; }
};

}  // namespace croniot::telemetry

#endif
