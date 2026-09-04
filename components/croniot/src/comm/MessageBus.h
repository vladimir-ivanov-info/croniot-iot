#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CommChannel.h"
#include "Result.h"
#include "Tasks/TaskBase.h"

namespace croniot {

class MessageBus {
public:
    static MessageBus& instance();

    void setDeviceUuid(const std::string& uuid) { deviceUuid_ = uuid; }
    const std::string& getDeviceUuid() const { return deviceUuid_; }

    void addChannel(std::unique_ptr<CommChannel> channel);

    bool startConnection(CommChannel::ConnectionReadyCallback onReady);
    bool startMessaging();
    bool hasServerAuthChannel() const { return serverAuthChannel() != nullptr; }

    Result registerDevice(const std::string& jsonPayload);
    Result login(const std::string& jsonPayload);
    Result registerSensorType(const std::string& jsonPayload);
    Result registerTaskType(const std::string& jsonPayload);

    Result publishSensorData(int sensorUid, const std::string& jsonValue);
    Result publishTaskProgressUpdate(const std::string& jsonPayload);

    void subscribeTaskCommand(int taskTypeUid, TaskBase* taskInstance);
    void subscribeTaskStateInfoSync(int taskTypeUid, TaskBase* taskInstance);

    // Test-only: clears all registered channels and the device UUID so this
    // process-wide singleton can be reused cleanly across independent test
    // cases (see croniot/test_host/MessageBusRoutingTest.cpp). Never call
    // this from production code.
    void resetForTesting() {
        channels_.clear();
        deviceUuid_.clear();
    }

private:
    MessageBus() = default;

    CommChannel* serverAuthChannel() const;

    std::string deviceUuid_;
    std::vector<std::unique_ptr<CommChannel>> channels_;
};

}
