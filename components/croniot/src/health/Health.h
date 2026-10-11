#ifndef CRONIOT_HEALTH_HEALTH_H
#define CRONIOT_HEALTH_HEALTH_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace croniot::health {

// Periodic device health report (plan §11.4 Fase 3 / §12.4 PR13): every
// CRONIOT_HEALTH_INTERVAL_SEC, collects heap/uptime/RSSI/reconnects/
// dropped-logs and per-partition free%/wear%/uplink-backlog/resends,
// then emits them as two compact croniot::log::event() calls ("health",
// "health_storage" - see HealthReport.h for why two and why compact).
// Deliberately reuses that exact pipeline rather than a new wire
// protocol: a health event is durable (Journal's Events stream) and
// gets drained by the same Uplink/ack/retry machinery as everything
// else, so "is the device still reporting" survives an offline stretch
// exactly like any other event does (plan §9.1 point 7's "dead man's
// switch": absence over time, not a synchronous heartbeat ack).
class Health {
public:
    static Health& instance();

    // Idempotent. Creates the drain task (no-op if
    // CONFIG_CRONIOT_HEALTH_ENABLE=n). Safe to call as soon as Log::init()
    // has run - does not require MessageBus/a connected channel, since a
    // health event that never leaves the Journal is still useful
    // (readable later, e.g. over BLE) and "device never got online" is
    // itself exactly the kind of thing this should be able to report.
    void start();

    // Plan §7.2's croniot::health::registerMetric(name, callback) -
    // project-defined metrics (e.g. pump_runtime_s_today) folded into a
    // third, optional "health_custom" event alongside the two built-in
    // ones. `valueFn` is called from the health task, not the caller's
    // own task - keep it cheap and non-blocking.
    void registerMetric(const std::string& name, std::function<double()> valueFn);

private:
    Health() = default;

    static void run(void* arg);
    void reportOnce();

    std::vector<std::pair<std::string, std::function<double()>>> customMetrics_;
    bool started_ = false;
};

}  // namespace croniot::health

#endif
