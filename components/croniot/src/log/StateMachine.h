#ifndef CRONIOT_LOG_STATEMACHINE_H
#define CRONIOT_LOG_STATEMACHINE_H

#include <functional>
#include <string>
#include <utility>

namespace croniot::log {

// Records the transition, not the step: setState(next, reason) replaces
// today's pattern of a free-form std::string state field that each project
// mutates by hand with no trace of *why* it changed. The sink is global and
// injectable (like Span/Counters) so this stays host-testable; it defaults
// to a no-op until init() (or a test) wires the real one.
using StateTransitionSink =
    std::function<void(const std::string& machineName, int fromState, int toState, const std::string& reason)>;

void setStateTransitionSink(StateTransitionSink sink);
void resetStateTransitionSinkForTesting();

// Exposed so setState() below can reach the current sink without every
// translation unit needing its own copy of the global.
StateTransitionSink& stateTransitionSink();

template <typename StateEnum>
class StateMachine {
public:
    StateMachine(std::string machineName, StateEnum initial)
        : machineName_(std::move(machineName)), state_(initial) {}

    StateEnum state() const { return state_; }

    void setState(StateEnum next, const std::string& reason) {
        StateEnum previous = state_;
        state_ = next;
        stateTransitionSink()(machineName_, static_cast<int>(previous), static_cast<int>(next), reason);
    }

private:
    std::string machineName_;
    StateEnum state_;
};

}  // namespace croniot::log

#endif
