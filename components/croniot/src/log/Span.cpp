#include "Span.h"

namespace croniot::log {

namespace {
SpanClock g_clock = [] { return uint64_t{0}; };
SpanSink g_sink = [](const std::string&, uint64_t) {};
}  // namespace

void setSpanClock(SpanClock clock) { g_clock = clock ? std::move(clock) : [] { return uint64_t{0}; }; }
void setSpanSink(SpanSink sink) { g_sink = sink ? std::move(sink) : SpanSink([](const std::string&, uint64_t) {}); }

void resetSpanHooksForTesting() {
    g_clock = [] { return uint64_t{0}; };
    g_sink = [](const std::string&, uint64_t) {};
}

Span::Span(std::string name) : name_(std::move(name)), startUs_(g_clock()) {}

Span::~Span() {
    uint64_t endUs = g_clock();
    uint64_t duration = endUs >= startUs_ ? endUs - startUs_ : 0;
    g_sink(name_, duration);
}

}  // namespace croniot::log
