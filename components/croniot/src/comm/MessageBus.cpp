#include "MessageBus.h"

#include "esp_log.h"

namespace croniot {

static const char* TAG = "MessageBus";

MessageBus& MessageBus::instance() {
    static MessageBus _instance;
    return _instance;
}

void MessageBus::addChannel(std::unique_ptr<CommChannel> channel) {
    channels_.push_back(std::move(channel));
}

CommChannel* MessageBus::serverAuthChannel() const {
    for (auto& ch : channels_) {
        if (ch->supportsServerAuth()) return ch.get();
    }
    return nullptr;
}

bool MessageBus::startConnection(CommChannel::ConnectionReadyCallback onReady) {
    if (channels_.empty()) {
        ESP_LOGE(TAG, "No channels configured");
        return false;
    }
    // If no channel supports server auth (BLE-only), fire onReady from the first
    // channel that connects so that sensors/tasks still get initialized.
    const bool hasAuth = hasServerAuthChannel();
    bool anyOk = false;
    bool callbackAssigned = false;
    for (auto& ch : channels_) {
        CommChannel::ConnectionReadyCallback callback;
        if (ch->supportsServerAuth()) {
            callback = onReady;
            callbackAssigned = true;
        } else if (!hasAuth && !callbackAssigned) {
            callback = onReady;
            callbackAssigned = true;
        }
        anyOk = ch->startConnection(callback) || anyOk;
    }
    return anyOk;
}

bool MessageBus::startMessaging() {
    bool anyOk = false;
    for (auto& ch : channels_) {
        anyOk = ch->startMessaging() || anyOk;
    }
    return anyOk;
}

Result MessageBus::registerDevice(const std::string& jsonPayload) {
    auto* ch = serverAuthChannel();
    if (!ch) return Result(false, "No server-auth channel");
    return ch->registerDevice(jsonPayload);
}

Result MessageBus::login(const std::string& jsonPayload) {
    auto* ch = serverAuthChannel();
    if (!ch) return Result(false, "No server-auth channel");
    return ch->login(jsonPayload);
}

Result MessageBus::registerSensorType(const std::string& jsonPayload) {
    auto* ch = serverAuthChannel();
    if (!ch) return Result(false, "No server-auth channel");
    return ch->registerSensorType(jsonPayload);
}

Result MessageBus::registerTaskType(const std::string& jsonPayload) {
    auto* ch = serverAuthChannel();
    if (!ch) return Result(false, "No server-auth channel");
    return ch->registerTaskType(jsonPayload);
}

Result MessageBus::publishSensorData(int sensorUid, const std::string& jsonValue) {
    Result last(false, "No channels");
    for (auto& ch : channels_) {
        last = ch->publishSensorData(deviceUuid_, sensorUid, jsonValue);
    }
    return last;
}

Result MessageBus::publishTaskProgressUpdate(const std::string& jsonPayload) {
    Result last(false, "No channels");
    for (auto& ch : channels_) {
        last = ch->publishTaskProgressUpdate(deviceUuid_, jsonPayload);
    }
    return last;
}

void MessageBus::subscribeTaskCommand(int taskTypeUid, TaskBase* taskInstance) {
    for (auto& ch : channels_) {
        ch->subscribeTaskCommand(deviceUuid_, taskTypeUid, taskInstance);
    }
}

void MessageBus::subscribeTaskStateInfoSync(int taskTypeUid, TaskBase* taskInstance) {
    for (auto& ch : channels_) {
        ch->subscribeTaskStateInfoSync(deviceUuid_, taskTypeUid, taskInstance);
    }
}

}
