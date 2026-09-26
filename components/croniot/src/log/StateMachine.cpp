#include "StateMachine.h"

namespace croniot::log {

namespace {
StateTransitionSink makeNoopSink() {
    return [](const std::string&, int, int, const std::string&) {};
}
}  // namespace

StateTransitionSink& stateTransitionSink() {
    static StateTransitionSink sink = makeNoopSink();
    return sink;
}

void setStateTransitionSink(StateTransitionSink sink) {
    stateTransitionSink() = sink ? std::move(sink) : makeNoopSink();
}

void resetStateTransitionSinkForTesting() { stateTransitionSink() = makeNoopSink(); }

}  // namespace croniot::log
