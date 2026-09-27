#include "LoggingCommChannel.h"

#include <cstdint>
#include <string>
#include <utility>

#include "esp_timer.h"

#include "log/Counters.h"
#include "log/Log.h"

namespace croniot {

namespace {
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
}  // namespace

LoggingCommChannel::LoggingCommChannel(std::unique_ptr<CommChannel> inner, std::string channelName)
    : inner_(std::move(inner)), channelName_(std::move(channelName)) {}

Result LoggingCommChannel::timed(const char* op, const std::function<Result()>& call) {
    uint64_t start = nowMs();
    Result result = call();
    uint64_t durationMs = nowMs() - start;
    if (result.success) {
        croniot::log::Counters::instance().increment(std::string(op) + "_ok");
    } else {
        croniot::log::event(
            "channel_op_failed", croniot::log::Level::Warn,
            {{"op", op}, {"ch", channelName_}, {"ms", std::to_string(durationMs)}, {"err", result.message}});
    }
    return result;
}

bool LoggingCommChannel::timedBool(const char* op, const std::function<bool()>& call) {
    uint64_t start = nowMs();
    bool ok = call();
    uint64_t durationMs = nowMs() - start;
    if (ok) {
        croniot::log::Counters::instance().increment(std::string(op) + "_ok");
    } else {
        croniot::log::event("channel_op_failed", croniot::log::Level::Warn,
                             {{"op", op}, {"ch", channelName_}, {"ms", std::to_string(durationMs)}});
    }
    return ok;
}

bool LoggingCommChannel::startConnection(ConnectionReadyCallback onReady) {
    return timedBool("startConnection", [&] { return inner_->startConnection(std::move(onReady)); });
}

bool LoggingCommChannel::startMessaging() {
    return timedBool("startMessaging", [&] { return inner_->startMessaging(); });
}

bool LoggingCommChannel::isConnected() const { return inner_->isConnected(); }

bool LoggingCommChannel::supportsServerAuth() const { return inner_->supportsServerAuth(); }

Result LoggingCommChannel::registerDevice(const std::string& jsonPayload) {
    return timed("registerDevice", [&] { return inner_->registerDevice(jsonPayload); });
}

Result LoggingCommChannel::login(const std::string& jsonPayload) {
    return timed("login", [&] { return inner_->login(jsonPayload); });
}

Result LoggingCommChannel::registerSensorType(const std::string& jsonPayload) {
    return timed("registerSensorType", [&] { return inner_->registerSensorType(jsonPayload); });
}

Result LoggingCommChannel::registerTaskType(const std::string& jsonPayload) {
    return timed("registerTaskType", [&] { return inner_->registerTaskType(jsonPayload); });
}

Result LoggingCommChannel::publishSensorData(const std::string& deviceUuid, int sensorUid,
                                              const std::string& jsonValue) {
    return timed("publishSensorData",
                 [&] { return inner_->publishSensorData(deviceUuid, sensorUid, jsonValue); });
}

Result LoggingCommChannel::publishTaskProgressUpdate(const std::string& deviceUuid,
                                                       const std::string& jsonPayload) {
    return timed("publishTaskProgressUpdate",
                 [&] { return inner_->publishTaskProgressUpdate(deviceUuid, jsonPayload); });
}

void LoggingCommChannel::subscribeTaskCommand(const std::string& deviceUuid, int taskTypeUid,
                                               TaskBase* taskInstance) {
    inner_->subscribeTaskCommand(deviceUuid, taskTypeUid, taskInstance);
}

void LoggingCommChannel::subscribeTaskStateInfoSync(const std::string& deviceUuid, int taskTypeUid,
                                                     TaskBase* taskInstance) {
    inner_->subscribeTaskStateInfoSync(deviceUuid, taskTypeUid, taskInstance);
}

Result LoggingCommChannel::publishLogBatch(const std::string& deviceUuid, const std::string& cbor) {
    return timed("publishLogBatch", [&] { return inner_->publishLogBatch(deviceUuid, cbor); });
}

Result LoggingCommChannel::publishDeviceEvent(const std::string& deviceUuid, const std::string& cbor) {
    return timed("publishDeviceEvent", [&] { return inner_->publishDeviceEvent(deviceUuid, cbor); });
}

Result LoggingCommChannel::publishStatus(const std::string& deviceUuid, const std::string& jsonPayload,
                                          bool retain) {
    return timed("publishStatus", [&] { return inner_->publishStatus(deviceUuid, jsonPayload, retain); });
}

void LoggingCommChannel::subscribeAck(const std::string& deviceUuid,
                                       std::function<void(const std::string& json)> callback) {
    inner_->subscribeAck(deviceUuid, std::move(callback));
}

void LoggingCommChannel::subscribeLogConfig(const std::string& deviceUuid,
                                             std::function<void(const std::string& json)> callback) {
    inner_->subscribeLogConfig(deviceUuid, std::move(callback));
}

}  // namespace croniot
