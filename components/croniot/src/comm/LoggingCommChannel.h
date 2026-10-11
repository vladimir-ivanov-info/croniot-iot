#ifndef CRONIOT_COMM_LOGGINGCOMMCHANNEL_H
#define CRONIOT_COMM_LOGGINGCOMMCHANNEL_H

#include <functional>
#include <memory>
#include <string>

#include "CommChannel.h"

namespace croniot {

// Decorator over any CommChannel (plan §9.1 pattern 2 / §12.4 PR13):
// measures duration and outcome of every Result/bool-returning operation
// without RemoteChannel or BleChannel changing a single line - ADR 0007
// already frames CommChannel as "add a transport = one new
// implementation, zero changes to sensors/tasks"; this is the
// cross-cutting counterpart, "observe a transport = one wrapper, zero
// changes to the transport or its callers". Install by wrapping at the
// MessageBus::addChannel() call site:
//   bus.addChannel(std::make_unique<LoggingCommChannel>(
//       std::make_unique<RemoteChannel>(cfg), "remote"));
//
// Deliberately does NOT wrap isConnected()/supportsServerAuth() (pure
// state queries, polled far more than once a minute - logging their
// result would violate the plan's own "no loguees lo que puedes contar"
// rule) or any subscribe*() registration call (nothing to measure; the
// callback's own invocations aren't intercepted - a project wanting that
// would wrap the callback itself, out of scope here). Every other
// operation: success increments a Counters entry ("<op>_ok" - cheap,
// no event, matches the same rule for the *expected* case), failure
// emits a "channel_op_failed" event with the op name, channel name,
// duration and error message - see LoggingCommChannel.cpp's timed()/
// timedBool() for the shared implementation both call through.
class LoggingCommChannel : public CommChannel {
public:
    LoggingCommChannel(std::unique_ptr<CommChannel> inner, std::string channelName);

    bool startConnection(ConnectionReadyCallback onReady) override;
    bool startMessaging() override;
    bool isConnected() const override;
    bool supportsServerAuth() const override;

    Result registerDevice(const std::string& jsonPayload) override;
    Result login(const std::string& jsonPayload) override;
    Result registerSensorType(const std::string& jsonPayload) override;
    Result registerTaskType(const std::string& jsonPayload) override;

    Result publishSensorData(const std::string& deviceUuid, int sensorUid,
                              const std::string& jsonValue) override;
    Result publishTaskProgressUpdate(const std::string& deviceUuid, const std::string& jsonPayload) override;

    void subscribeTaskCommand(const std::string& deviceUuid, int taskTypeUid,
                               TaskBase* taskInstance) override;
    void subscribeTaskStateInfoSync(const std::string& deviceUuid, int taskTypeUid,
                                     TaskBase* taskInstance) override;

    Result publishLogBatch(const std::string& deviceUuid, const std::string& cbor) override;
    Result publishDeviceEvent(const std::string& deviceUuid, const std::string& cbor) override;
    Result publishStatus(const std::string& deviceUuid, const std::string& jsonPayload, bool retain) override;

    void subscribeAck(const std::string& deviceUuid,
                       std::function<void(const std::string& json)> callback) override;
    void subscribeLogConfig(const std::string& deviceUuid,
                             std::function<void(const std::string& json)> callback) override;

private:
    Result timed(const char* op, const std::function<Result()>& call);
    bool timedBool(const char* op, const std::function<bool()>& call);

    std::unique_ptr<CommChannel> inner_;
    std::string channelName_;
};

}  // namespace croniot

#endif
