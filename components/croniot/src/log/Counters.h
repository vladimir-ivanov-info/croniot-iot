#ifndef CRONIOT_LOG_COUNTERS_H
#define CRONIOT_LOG_COUNTERS_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace croniot::log {

// "No loguees lo que puedes contar": named counters for anything that
// happens more than once a minute in normal operation (mqtt.reconnect,
// queue_full, publish_failed...). The first occurrence still gets a log
// line at the call site if it matters; the rest just increment here and
// travel in the periodic health report (Fase 3).
class Counters {
public:
    static Counters& instance() {
        static Counters instance;
        return instance;
    }

    void increment(const std::string& name, uint32_t by = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        counts_[name] += by;
    }

    uint32_t get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = counts_.find(name);
        return it == counts_.end() ? 0 : it->second;
    }

    std::map<std::string, uint32_t> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return counts_;
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        counts_.clear();
    }

private:
    Counters() = default;
    mutable std::mutex mutex_;
    std::map<std::string, uint32_t> counts_;
};

}  // namespace croniot::log

#define CRONIOT_COUNT(name) ::croniot::log::Counters::instance().increment(#name)

#endif
