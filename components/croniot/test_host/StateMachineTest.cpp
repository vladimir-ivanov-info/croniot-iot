#include <gtest/gtest.h>

#include "log/StateMachine.h"

using croniot::log::resetStateTransitionSinkForTesting;
using croniot::log::setStateTransitionSink;
using croniot::log::StateMachine;

enum class TaskState { Pending, Running, Completed, Failed };

class StateMachineTest : public ::testing::Test {
protected:
    void SetUp() override { resetStateTransitionSinkForTesting(); }
    void TearDown() override { resetStateTransitionSinkForTesting(); }
};

TEST_F(StateMachineTest, StartsAtTheGivenInitialState) {
    StateMachine<TaskState> machine("task.water", TaskState::Pending);
    EXPECT_EQ(machine.state(), TaskState::Pending);
}

TEST_F(StateMachineTest, SetStateUpdatesCurrentState) {
    StateMachine<TaskState> machine("task.water", TaskState::Pending);
    machine.setState(TaskState::Running, "started");
    EXPECT_EQ(machine.state(), TaskState::Running);
}

TEST_F(StateMachineTest, TransitionSinkReceivesNameFromToAndReason) {
    struct Transition {
        std::string machineName;
        int from;
        int to;
        std::string reason;
    };
    std::vector<Transition> transitions;
    setStateTransitionSink([&transitions](const std::string& name, int from, int to, const std::string& reason) {
        transitions.push_back({name, from, to, reason});
    });

    StateMachine<TaskState> machine("task.water", TaskState::Pending);
    machine.setState(TaskState::Running, "worker picked it up");
    machine.setState(TaskState::Completed, "valve closed");

    ASSERT_EQ(transitions.size(), 2u);
    EXPECT_EQ(transitions[0].machineName, "task.water");
    EXPECT_EQ(transitions[0].from, static_cast<int>(TaskState::Pending));
    EXPECT_EQ(transitions[0].to, static_cast<int>(TaskState::Running));
    EXPECT_EQ(transitions[0].reason, "worker picked it up");
    EXPECT_EQ(transitions[1].from, static_cast<int>(TaskState::Running));
    EXPECT_EQ(transitions[1].to, static_cast<int>(TaskState::Completed));
}

TEST_F(StateMachineTest, IsSafeToUseWithNoSinkConfigured) {
    StateMachine<TaskState> machine("unconfigured", TaskState::Pending);
    machine.setState(TaskState::Failed, "no crash expected");
    EXPECT_EQ(machine.state(), TaskState::Failed);
}
