#pragma once

#include <atomic>
#include <map>
#include <string>

#include "CommChannel.h"
#include "CroniotConfig.h"

namespace croniot {

class BleChannel : public CommChannel {
public:
    BleChannel(const std::string& deviceUuid,
               const CroniotConfig::BleCfg& cfg);

    bool startConnection(ConnectionReadyCallback onReady) override;
    bool startMessaging() override;
    bool isConnected() const override;
    bool supportsServerAuth() const override { return false; }

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

    void onTaskCommandWrite(const std::string& payload);
    void onTaskStateSyncWrite(const std::string& payload);

    void startAdvertising();

    void setConnHandle(uint16_t handle) { connHandle_ = handle; }
    uint16_t connHandle() const { return connHandle_; }

    void setSensorDataAttrHandle(uint16_t h)    { hSensorData_ = h; }
    void setTaskProgressAttrHandle(uint16_t h)  { hTaskProgress_ = h; }
    uint16_t sensorDataAttrHandle()   const { return hSensorData_; }
    uint16_t taskProgressAttrHandle() const { return hTaskProgress_; }

    const std::string& deviceInfoJson() const { return deviceInfoJson_; }

    static BleChannel* instance() { return s_instance_; }

private:
    static BleChannel* s_instance_;

    static void hostTaskTrampoline(void* arg);
    static void onHostSync();
    static void onHostReset(int reason);

    void rebuildDeviceInfoJson();

    std::string deviceUuid_;
    std::string deviceName_;
    std::string deviceInfoJson_;

    std::atomic<uint16_t> connHandle_{0xFFFF};

    uint16_t hSensorData_ = 0;
    uint16_t hTaskProgress_ = 0;

    std::map<int, TaskBase*> taskCommandHandlers_;
    std::map<int, TaskBase*> taskStateSyncHandlers_;

    ConnectionReadyCallback onReady_;
    std::atomic<bool> readyFired_{false};
    std::atomic<bool> hostSynced_{false};
};

}
