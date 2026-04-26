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

}
