#pragma once

#include <string>
#include <map>
#include <functional>
#include "MqttController.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/semphr.h"
#include "Tasks/TaskBase.h"

class WifiMqttController : public MqttController {
public:
    static WifiMqttController& instance() {
        static WifiMqttController _instance;
        return _instance;
    }

    bool init() override;
    Result publish(const std::string& topic, const std::string& message) override;
    void registerCallback(const std::string& topic, TaskBase* taskInstance) override;
    void registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance) override;

private:
    esp_mqtt_client_handle_t mqttClient = nullptr;
    SemaphoreHandle_t mutex = nullptr;

    std::map<std::string, TaskBase*> topicTaskMap;
    std::map<std::string, TaskBase*> topicTaskStateInfoSyncMap;

    static void mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void handleMessage(const std::string& topic, const std::string& payload);

    bool reconnecting = false;
    uint64_t lastDisconnectedTimestamp = 0;

    struct MqttMsg {
        std::string topic;
        std::string payload;
    };

    static void WorkerTask(void* arg);
    void processInWorker(const std::string& topic, const std::string& payload);

    QueueHandle_t msgQueue = nullptr;   // cola de punteros MqttMsg*
    //esp_mqtt_client_handle_t mqttClient = nullptr;
};
