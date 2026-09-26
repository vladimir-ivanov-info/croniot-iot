#ifndef CRONIOT_LOG_SPAN_H
#define CRONIOT_LOG_SPAN_H

#include <cstdint>
#include <functional>
#include <string>

namespace croniot::log {

// RAII duration measurement: one line at the call site instead of a
// "start" log plus an "end" log with a manually-computed delta. Safe to
// use before init() - clock and sink default to no-ops, and init() (or a
// test) wires the real ones once via the setters below.
using SpanClock = std::function<uint64_t()>;          // now, in microseconds
using SpanSink = std::function<void(const std::string& name, uint64_t durationUs)>;

void setSpanClock(SpanClock clock);
void setSpanSink(SpanSink sink);

// Test-only: restores both hooks to their no-op defaults.
void resetSpanHooksForTesting();

class Span {
public:
    explicit Span(std::string name);
    ~Span();

    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;

private:
    std::string name_;
    uint64_t startUs_;
};

}  // namespace croniot::log

// Hides the unique-variable-name boilerplate a bare Span would need at
// each call site: CRONIOT_SPAN("mqtt.publish");
#define CRONIOT_SPAN(name) ::croniot::log::Span CRONIOT_SPAN_CONCAT(croniot_span_, __LINE__)(name)
#define CRONIOT_SPAN_CONCAT(a, b) CRONIOT_SPAN_CONCAT_INNER(a, b)
#define CRONIOT_SPAN_CONCAT_INNER(a, b) a##b

#endif
