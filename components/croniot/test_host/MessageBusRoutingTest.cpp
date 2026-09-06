#include <gtest/gtest.h>

#include <memory>

#include "comm/MessageBus.h"
#include "fakes/FakeCommChannel.h"

using croniot::MessageBus;

class MessageBusRoutingTest : public ::testing::Test {
protected:
    void SetUp() override {
        MessageBus::instance().resetForTesting();
    }

    // Adds a channel and returns an observer pointer (MessageBus owns it).
    FakeCommChannel* addChannel(bool supportsAuth, const std::string& name) {
        auto channel = std::make_unique<FakeCommChannel>(supportsAuth, name);
        FakeCommChannel* raw = channel.get();
        MessageBus::instance().addChannel(std::move(channel));
        return raw;
    }
};

TEST_F(MessageBusRoutingTest, RegisterDeviceGoesOnlyToAuthChannel) {
    FakeCommChannel* authChannel = addChannel(/*supportsAuth=*/true, "auth");
    FakeCommChannel* bleChannel = addChannel(/*supportsAuth=*/false, "ble");

    MessageBus::instance().registerDevice("{}");

    EXPECT_EQ(authChannel->registerDeviceCalls, 1);
    EXPECT_EQ(bleChannel->registerDeviceCalls, 0);
}

TEST_F(MessageBusRoutingTest, LoginGoesOnlyToAuthChannel) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().login("{}");

    EXPECT_EQ(authChannel->loginCalls, 1);
    EXPECT_EQ(bleChannel->loginCalls, 0);
}

TEST_F(MessageBusRoutingTest, RegisterSensorAndTaskTypeGoOnlyToAuthChannel) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().registerSensorType("{}");
    MessageBus::instance().registerTaskType("{}");

    EXPECT_EQ(authChannel->registerSensorTypeCalls, 1);
    EXPECT_EQ(authChannel->registerTaskTypeCalls, 1);
    EXPECT_EQ(bleChannel->registerSensorTypeCalls, 0);
    EXPECT_EQ(bleChannel->registerTaskTypeCalls, 0);
}

TEST_F(MessageBusRoutingTest, NoAuthChannelMeansRegisterCallsFail) {
    addChannel(false, "ble-only");

    Result result = MessageBus::instance().registerDevice("{}");

    EXPECT_FALSE(result.success);
}

TEST_F(MessageBusRoutingTest, PublishSensorDataBroadcastsToAllChannels) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().publishSensorData(42, "\"23.5\"");

    EXPECT_EQ(authChannel->publishSensorDataCalls, 1);
    EXPECT_EQ(bleChannel->publishSensorDataCalls, 1);
    EXPECT_EQ(authChannel->lastSensorUid, 42);
    EXPECT_EQ(bleChannel->lastSensorUid, 42);
}

TEST_F(MessageBusRoutingTest, PublishTaskProgressUpdateBroadcastsToAllChannels) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().publishTaskProgressUpdate("{}");

    EXPECT_EQ(authChannel->publishTaskProgressCalls, 1);
    EXPECT_EQ(bleChannel->publishTaskProgressCalls, 1);
}

TEST_F(MessageBusRoutingTest, SubscribeTaskCommandRegistersOnAllChannels) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().subscribeTaskCommand(/*taskTypeUid=*/1, /*taskInstance=*/nullptr);

    EXPECT_EQ(authChannel->subscribeTaskCommandCalls, 1);
    EXPECT_EQ(bleChannel->subscribeTaskCommandCalls, 1);
    EXPECT_EQ(authChannel->lastSubscribedTaskTypeUid, 1);
    EXPECT_EQ(bleChannel->lastSubscribedTaskTypeUid, 1);
}

TEST_F(MessageBusRoutingTest, SubscribeTaskStateInfoSyncRegistersOnAllChannels) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");

    MessageBus::instance().subscribeTaskStateInfoSync(1, nullptr);

    EXPECT_EQ(authChannel->subscribeTaskStateInfoSyncCalls, 1);
    EXPECT_EQ(bleChannel->subscribeTaskStateInfoSyncCalls, 1);
}

// BLE-only mode (mirrors the watering-system feature/ble-password branch,
// which configures only ChannelType::Ble - no channel supports server auth):
// startConnection's onReady callback should fire from the first channel
// that connects, instead of waiting forever for a server-auth channel.
TEST_F(MessageBusRoutingTest, NoAuthChannelFiresReadyFromFirstConnectingChannel) {
    FakeCommChannel* bleChannel = addChannel(/*supportsAuth=*/false, "ble-only");

    bool readyFired = false;
    bool started = MessageBus::instance().startConnection([&readyFired] { readyFired = true; });

    EXPECT_TRUE(started);
    EXPECT_TRUE(readyFired);
    EXPECT_EQ(bleChannel->startConnectionCalls, 1);
}

TEST_F(MessageBusRoutingTest, AuthChannelPresentGetsTheReadyCallbackNotBleChannel) {
    FakeCommChannel* authChannel = addChannel(true, "auth");
    FakeCommChannel* bleChannel = addChannel(false, "ble");
    bleChannel->connectsSuccessfully = false;  // BLE connects later/never in this scenario

    bool readyFired = false;
    MessageBus::instance().startConnection([&readyFired] { readyFired = true; });

    // Auth channel connects successfully and is the one wired to onReady.
    EXPECT_TRUE(readyFired);
    EXPECT_EQ(authChannel->startConnectionCalls, 1);
}

TEST_F(MessageBusRoutingTest, NoChannelsConfiguredFailsToStart) {
    bool started = MessageBus::instance().startConnection([] {});
    EXPECT_FALSE(started);
}
