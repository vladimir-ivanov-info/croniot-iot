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

private:
    MessageBus() = default;

    CommChannel* serverAuthChannel() const;

    std::string deviceUuid_;
    std::vector<std::unique_ptr<CommChannel>> channels_;
};

}
