#include "RemoteChannel.h"

#include "esp_log.h"

#include "network/NetworkManager.h"
#include "network/connection_provider/NetworkConnectionProvider.h"
#include "network/connection_provider/Sim7600NetworkConnectionController.h"
#include "network/connection_provider/WifiNetworkConnectionController.h"
#include "network/http/Sim7600HttpController.h"
#include "network/http/WifiHttpController.h"
#include "network/http/HttpProvider.h"
#include "network/mqtt/Sim7600MqttController.h"
#include "network/mqtt/WifiMqttController.h"

namespace croniot {

static const char* TAG = "RemoteChannel";

namespace {

const std::string ROUTE_REGISTER_CLIENT      = "/api/register_client";
const std::string ROUTE_IOT_LOGIN            = "/api/iot/login";
const std::string ROUTE_REGISTER_SENSOR_TYPE = "/api/register_sensor_type";
const std::string ROUTE_REGISTER_TASK_TYPE   = "/api/register_task_type";

// Telemetry topics (plan §4). Status is shared between the LWT
// (WifiMqttController::init() sets "offline" on that same topic) and
// this channel's own retained "online"/birth publish.
std::string topicLogs(const std::string& deviceUuid)   { return "/iot_to_server/logs/" + deviceUuid; }
std::string topicEvents(const std::string& deviceUuid) { return "/iot_to_server/events/" + deviceUuid; }
std::string topicSensorBatch(const std::string& deviceUuid) { return "/iot_to_server/sensor_batch/" + deviceUuid; }
std::string topicStatus(const std::string& deviceUuid) { return "/iot_to_server/status/" + deviceUuid; }
std::string topicAck(const std::string& deviceUuid)    { return "/server/" + deviceUuid + "/ack"; }
std::string topicLogConfig(const std::string& deviceUuid) { return "/server/" + deviceUuid + "/log_config"; }

}

RemoteChannel::RemoteChannel(const CroniotConfig::RemoteCfg& cfg) : cfg_(cfg) {
    if (cfg_.transport == RemoteTransport::Wifi) {
        http_    = &WifiHttpController::instance();
        mqtt_    = &WifiMqttController::instance();
        network_ = &WifiNetworkConnectionController::instance();
    } else {
        http_    = &Sim7600HttpController::instance();
        mqtt_    = &Sim7600MqttController::instance();
        network_ = &Sim7600NetworkConnectionController::getInstance();
    }

    // CurrentDateTimeController still uses HttpProvider::get() internally.
    HttpProvider::set(http_);

    NetworkManager::instance().setServerAddress(cfg_.serverAddress, cfg_.serverHttpPort, cfg_.serverMqttPort);
    NetworkManager::instance().setWifiCredentials(cfg_.wifiSsid, cfg_.wifiPassword);
}

bool RemoteChannel::startConnection(ConnectionReadyCallback onReady) {
    auto wifiCb = [onReady](const std::string& ssid) {
        ESP_LOGI(TAG, "Connection ready: %s", ssid.c_str());
        if (onReady) onReady();
    };
    return NetworkConnectionProvider::init(network_, wifiCb);
}

bool RemoteChannel::startMessaging() {
    if (!mqtt_) return false;
    return mqtt_->init();
}

bool RemoteChannel::isConnected() const {
    return network_ && network_->connectedToNetwork();
}

Result RemoteChannel::registerDevice(const std::string& jsonPayload) {
    return http_->sendHttpPost(jsonPayload, ROUTE_REGISTER_CLIENT);
}

Result RemoteChannel::login(const std::string& jsonPayload) {
    return http_->sendHttpPost(jsonPayload, ROUTE_IOT_LOGIN);
}

Result RemoteChannel::registerSensorType(const std::string& jsonPayload) {
    return http_->sendHttpPost(jsonPayload, ROUTE_REGISTER_SENSOR_TYPE);
}

Result RemoteChannel::registerTaskType(const std::string& jsonPayload) {
    return http_->sendHttpPost(jsonPayload, ROUTE_REGISTER_TASK_TYPE);
}

Result RemoteChannel::publishSensorData(const std::string& deviceUuid,
                                        int sensorUid,
                                        const std::string& jsonValue) {
    std::string topic = "/" + deviceUuid + "/sensor_data";
    return mqtt_->publish(topic, jsonValue);
}

Result RemoteChannel::publishTaskProgressUpdate(const std::string& deviceUuid,
                                                const std::string& jsonPayload) {
    std::string topic = "/iot_to_server/task_progress_update/" + deviceUuid;
    return mqtt_->publish(topic, jsonPayload);
}

void RemoteChannel::subscribeTaskCommand(const std::string& deviceUuid,
                                         int taskTypeUid,
                                         TaskBase* taskInstance) {
    std::string topic = "/server/" + deviceUuid + "/task_type/" + std::to_string(taskTypeUid);
    mqtt_->registerCallback(topic, taskInstance);
}

void RemoteChannel::subscribeTaskStateInfoSync(const std::string& deviceUuid,
                                               int taskTypeUid,
                                               TaskBase* taskInstance) {
    std::string topic = "/server/" + deviceUuid + "/task_state_info_sync/" + std::to_string(taskTypeUid);
    mqtt_->registerCallbackTaskStateInfoSync(topic, taskInstance);
}

Result RemoteChannel::publishLogBatch(const std::string& deviceUuid, const std::string& cbor) {
    // QoS 1, not the sensor-data path's QoS 2 (plan §4): logs/events are
    // idempotent by (device, boot, stream, seq) already, so QoS 2's extra
    // round trip (PUBREC/PUBREL/PUBCOMP) buys nothing here.
    return mqtt_->publishWithOptions(topicLogs(deviceUuid), cbor, /*qos=*/1, /*retain=*/false);
}

Result RemoteChannel::publishDeviceEvent(const std::string& deviceUuid, const std::string& cbor) {
    return mqtt_->publishWithOptions(topicEvents(deviceUuid), cbor, /*qos=*/1, /*retain=*/false);
}

Result RemoteChannel::publishSensorBatch(const std::string& deviceUuid, const std::string& cbor) {
    // Same QoS 1 reasoning as publishLogBatch above - dedup is by
    // (device, boot, stream, seq) via the Journal cursor, not transport
    // QoS, and this is a batch already (not the legacy per-reading QoS
    // 2 publishSensorData path, which stays untouched for Immediate-
    // policy sensors).
    return mqtt_->publishWithOptions(topicSensorBatch(deviceUuid), cbor, /*qos=*/1, /*retain=*/false);
}

Result RemoteChannel::publishStatus(const std::string& deviceUuid, const std::string& jsonPayload,
                                    bool retain) {
    return mqtt_->publishWithOptions(topicStatus(deviceUuid), jsonPayload, /*qos=*/1, retain);
}

void RemoteChannel::subscribeAck(const std::string& deviceUuid,
                                 std::function<void(const std::string&)> callback) {
    mqtt_->registerRawCallback(topicAck(deviceUuid), std::move(callback));
}

void RemoteChannel::subscribeLogConfig(const std::string& deviceUuid,
                                       std::function<void(const std::string&)> callback) {
    mqtt_->registerRawCallback(topicLogConfig(deviceUuid), std::move(callback));
}

}
