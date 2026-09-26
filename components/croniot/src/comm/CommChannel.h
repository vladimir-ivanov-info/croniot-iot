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

    // Telemetry uplink (plan §4/§5). `cbor` is a pre-built
    // telemetry::encodeBatchEnvelope()/LogRecord-shaped payload (opaque
    // binary here - this interface stays payload-agnostic, same as
    // publishSensorData's jsonValue). BLE's implementation of these is a
    // documented no-op until Tanda E's LOG_CONFIG/LOG_STREAM
    // characteristics land (plan §5 point 11: over BLE the phone is the
    // destination, not a relay to this same protocol).
    virtual Result publishLogBatch(const std::string& deviceUuid, const std::string& cbor) = 0;
    virtual Result publishDeviceEvent(const std::string& deviceUuid, const std::string& cbor) = 0;

    // Retained status/birth publish (plan §4: LWT sets "offline",
    // this sets the online/birth counterpart - `{boot, reset_reason, fw}`
    // as the jsonPayload).
    virtual Result publishStatus(const std::string& deviceUuid, const std::string& jsonPayload,
                                  bool retain) = 0;

    // Application-level ack (plan §5 point 3 - NOT the MQTT PUBACK) and
    // remote log-level control (plan §4). Both are plain JSON, so they
    // go through the same generic-callback plumbing as MqttController::
    // registerRawCallback() rather than the TaskBase*-shaped
    // subscribeTaskCommand/subscribeTaskStateInfoSync above.
    virtual void subscribeAck(const std::string& deviceUuid,
                              std::function<void(const std::string& json)> callback) = 0;
    virtual void subscribeLogConfig(const std::string& deviceUuid,
                                    std::function<void(const std::string& json)> callback) = 0;
};

}
