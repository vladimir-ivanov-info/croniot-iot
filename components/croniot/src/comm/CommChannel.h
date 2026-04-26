#pragma once

#include <functional>
#include <string>
#include "Result.h"
#include "Tasks/TaskBase.h"

namespace croniot {

class CommChannel {
public:
    using ConnectionReadyCallback = std::function<void()>;

    virtual ~CommChannel() = default;

    virtual bool startConnection(ConnectionReadyCallback onReady) = 0;
    virtual bool startMessaging() = 0;
    virtual bool isConnected() const = 0;

    virtual bool supportsServerAuth() const = 0;

    virtual Result registerDevice(const std::string& jsonPayload) = 0;
    virtual Result login(const std::string& jsonPayload) = 0;
    virtual Result registerSensorType(const std::string& jsonPayload) = 0;
    virtual Result registerTaskType(const std::string& jsonPayload) = 0;

    virtual Result publishSensorData(const std::string& deviceUuid,
                                     int sensorUid,
                                     const std::string& jsonValue) = 0;

    virtual Result publishTaskProgressUpdate(const std::string& deviceUuid,
                                             const std::string& jsonPayload) = 0;

    virtual void subscribeTaskCommand(const std::string& deviceUuid,
                                      int taskTypeUid,
                                      TaskBase* taskInstance) = 0;

    virtual void subscribeTaskStateInfoSync(const std::string& deviceUuid,
                                            int taskTypeUid,
                                            TaskBase* taskInstance) = 0;
};

}
