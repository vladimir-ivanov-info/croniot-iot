#pragma once

#include <string>
#include <utility>

#include "comm/CommChannel.h"

// Test-only fake CommChannel: records every call instead of touching real
// BLE/WiFi/MQTT, so MessageBus's routing rules can be tested against the
// real MessageBus singleton (see MessageBusRoutingTest.cpp).
class FakeCommChannel : public croniot::CommChannel {
public:
    explicit FakeCommChannel(bool supportsAuth, std::string name = "fake")
        : supportsAuth_(supportsAuth), name(std::move(name)) {}

    bool startConnection(ConnectionReadyCallback onReady) override {
        startConnectionCalls++;
        onReadyCallback = onReady;
        if (connectsSuccessfully && onReady) onReady();
        return connectsSuccessfully;
    }

    bool startMessaging() override {
        startMessagingCalls++;
        return true;
    }

    bool isConnected() const override { return connectsSuccessfully; }
    bool supportsServerAuth() const override { return supportsAuth_; }

    Result registerDevice(const std::string& jsonPayload) override {
        registerDeviceCalls++;
        return Result(true, "");
    }

    Result login(const std::string& jsonPayload) override {
        loginCalls++;
        return Result(true, "");
    }

    Result registerSensorType(const std::string& jsonPayload) override {
        registerSensorTypeCalls++;
        return Result(true, "");
    }

    Result registerTaskType(const std::string& jsonPayload) override {
        registerTaskTypeCalls++;
        return Result(true, "");
    }

    Result publishSensorData(const std::string& deviceUuid,
                              int sensorUid,
                              const std::string& jsonValue) override {
        publishSensorDataCalls++;
        lastSensorUid = sensorUid;
        return Result(true, "");
    }

    Result publishTaskProgressUpdate(const std::string& deviceUuid,
                                      const std::string& jsonPayload) override {
        publishTaskProgressCalls++;
        return Result(true, "");
    }

    void subscribeTaskCommand(const std::string& deviceUuid,
                               int taskTypeUid,
                               TaskBase* taskInstance) override {
        subscribeTaskCommandCalls++;
        lastSubscribedTaskTypeUid = taskTypeUid;
    }

    void subscribeTaskStateInfoSync(const std::string& deviceUuid,
                                     int taskTypeUid,
                                     TaskBase* taskInstance) override {
        subscribeTaskStateInfoSyncCalls++;
    }

    // Test configuration.
    bool connectsSuccessfully = true;

    // Recorded state, for assertions.
    std::string name;
    int startConnectionCalls = 0;
    int startMessagingCalls = 0;
    int registerDeviceCalls = 0;
    int loginCalls = 0;
    int registerSensorTypeCalls = 0;
    int registerTaskTypeCalls = 0;
    int publishSensorDataCalls = 0;
    int publishTaskProgressCalls = 0;
    int subscribeTaskCommandCalls = 0;
    int subscribeTaskStateInfoSyncCalls = 0;
    int lastSensorUid = -1;
    int lastSubscribedTaskTypeUid = -1;
    ConnectionReadyCallback onReadyCallback;

private:
    bool supportsAuth_;
};
