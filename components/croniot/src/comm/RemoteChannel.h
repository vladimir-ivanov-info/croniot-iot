#pragma once

#include "CommChannel.h"
#include "CroniotConfig.h"
#include "network/connection_provider/NetworkConnectionControllerBase.h"
#include "network/http/HttpController.h"
#include "network/mqtt/MqttController.h"

namespace croniot {

class RemoteChannel : public CommChannel {
public:
    explicit RemoteChannel(const CroniotConfig::RemoteCfg& cfg);

    bool startConnection(ConnectionReadyCallback onReady) override;
    bool startMessaging() override;
    bool isConnected() const override;
    bool supportsServerAuth() const override { return true; }

    Result registerDevice(const std::string& jsonPayload) override;
    Result login(const std::string& jsonPayload) override;
    Result registerSensorType(const std::string& jsonPayload) override;
    Result registerTaskType(const std::string& jsonPayload) override;

    Result publishSensorData(const std::string& deviceUuid,
                             int sensorUid,
                             const std::string& jsonValue) override;

    Result publishTaskProgressUpdate(const std::string& deviceUuid,
                                     const std::string& jsonPayload) override;

    void subscribeTaskCommand(const std::string& deviceUuid,
                              int taskTypeUid,
                              TaskBase* taskInstance) override;

    void subscribeTaskStateInfoSync(const std::string& deviceUuid,
                                    int taskTypeUid,
                                    TaskBase* taskInstance) override;

private:
    CroniotConfig::RemoteCfg cfg_;
    HttpController* http_ = nullptr;
    MqttController* mqtt_ = nullptr;
    NetworkConnectionControllerBase* network_ = nullptr;
};

}
