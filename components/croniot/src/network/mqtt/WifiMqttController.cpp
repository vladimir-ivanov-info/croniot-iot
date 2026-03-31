#include "esp_timer.h"
#include <inttypes.h>  // para PRId32


#include <memory>
#include "WifiMqttController.h"
#include "network/NetworkManager.h"
#include "Tasks/TaskController.h"

#define TAG "WifiMqttController"

bool WifiMqttController::init() {
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
    }


    msgQueue = xQueueCreate(10, sizeof(MqttMsg*));
    configASSERT(msgQueue);
    xTaskCreatePinnedToCore(
        &WifiMqttController::WorkerTask, "mqtt_worker",
        4096, this, 9, nullptr, tskNO_AFFINITY
    );

    static std::string uriStr;  // static para que el puntero siga válido al iniciar MQTT
   // uriStr = "mqtt://" + NetworkManager::instance().serverAddress + ":" + std::to_string(NetworkManager::instance().serverMqttPort);
    //uriStr = "mqtt://51.77.195.204:" + std::to_string(NetworkManager::instance().serverMqttPort);

    //TODOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
    //uriStr = "mqtt://192.168.50.163:" + std::to_string(NetworkManager::instance().serverMqttPort); //TODO
    uriStr = "mqtt://57.131.29.79:" + std::to_string(NetworkManager::instance().serverMqttPort); //TODO

    esp_mqtt_client_config_t config = {};
    config.broker.address.uri = uriStr.c_str();  // ⚠️ Usar 'broker.address.uri' en ESP-IDF 5.x


    config.task.priority   = 10;
    config.task.stack_size = 8192;

    config.session.keepalive = 60;
    config.session.message_retransmit_timeout = 5000;
    config.network.reconnect_timeout_ms = 2000;

    config.buffer.size     = 2048;   // RX
    config.buffer.out_size = 2048;   // TX

    mqttClient = esp_mqtt_client_init(&config);

    // Cast explícito porque ESP_EVENT_ANY_ID es int, y esp_mqtt_event_id_t es enum class
    esp_mqtt_client_register_event(
        mqttClient,
        static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
        &mqttEventHandler,
        this
    );

    ESP_LOGI(TAG, "Connecting to MQTT... %s", uriStr.c_str());
    return esp_mqtt_client_start(mqttClient) == ESP_OK;
}

Result WifiMqttController::publish(const std::string& topic, const std::string& message) {

    //ESP_LOGI(TAG, "PUBLISHING MQTT %s %s ...", topic.c_str(), message.c_str());

    if (!mqttClient){
        ESP_LOGI(TAG, "MQTT not initialized...");
        return Result(false, "MQTT not initialized");
    } else {
        //ESP_LOGI(TAG, "MQTT initialized...");
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        int msgId = esp_mqtt_client_publish(
            mqttClient, topic.c_str(), message.c_str(), 0, 2, 0
        );
        xSemaphoreGive(mutex);
        if (msgId >= 0) {
            return Result(true, "Published successfully");
        } else {
            ESP_LOGW(TAG, "Publish failed, msgId: %d", msgId);
            return Result(false, "Publish failed");
        }
    }

    ESP_LOGW(TAG, "MQTT publish mutex timeout");
    return Result(false, "MQTT publish mutex timeout");

}

void WifiMqttController::registerCallback(const std::string& topic, TaskBase* taskInstance) {
    topicTaskMap[topic] = taskInstance;
    //topicTaskStateInfoSyncMap[topic] = taskInstance;

    if (mqttClient) {
        int msgId = esp_mqtt_client_subscribe(mqttClient, topic.c_str(), 1);
        ESP_LOGI(TAG, "Subscribed to topic: %s (msgId: %d)", topic.c_str(), msgId);
    }
}

void WifiMqttController::registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance){
    //topicTaskMap[topic] = taskInstance;
    topicTaskStateInfoSyncMap[topic] = taskInstance;

    if (mqttClient) {
        int msgId = esp_mqtt_client_subscribe(mqttClient, topic.c_str(), 1);
        ESP_LOGI(TAG, "Subscribed to topic: %s (msgId: %d)", topic.c_str(), msgId);
    }
}

void WifiMqttController::mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* controller = static_cast<WifiMqttController*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT connected");
            controller->initialized = true;
            controller->reconnecting = false;

            // Re-subscribe all topics after reconnection
            for (const auto& pair : controller->topicTaskMap) {
                esp_mqtt_client_subscribe(controller->mqttClient, pair.first.c_str(), 1);
                ESP_LOGI(TAG, "Re-subscribed to: %s", pair.first.c_str());
            }
            for (const auto& pair : controller->topicTaskStateInfoSyncMap) {
                esp_mqtt_client_subscribe(controller->mqttClient, pair.first.c_str(), 1);
                ESP_LOGI(TAG, "Re-subscribed to: %s", pair.first.c_str());
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            controller->initialized = false;
            controller->lastDisconnectedTimestamp = esp_timer_get_time();

            if (!controller->reconnecting) {
                controller->reconnecting = true;
                ESP_LOGI(TAG, "Reconnecting to MQTT in 2s...");
                esp_timer_handle_t reconnect_timer;
                esp_timer_create_args_t timer_args = {};
                timer_args.callback = [](void* arg) {
                    auto* self = static_cast<WifiMqttController*>(arg);
                    esp_mqtt_client_start(self->mqttClient);
                };
                timer_args.arg = controller;
                timer_args.name = "mqtt_reconn";
                timer_args.dispatch_method = ESP_TIMER_TASK;
                esp_timer_create(&timer_args, &reconnect_timer);
                esp_timer_start_once(reconnect_timer, 2000000);  // 2s en microsegundos
            }
            break;

        case MQTT_EVENT_DATA:
            if (event->topic_len > 0 && event->data_len > 0) {
                std::string topic(event->topic, event->topic_len);
                std::string payload(event->data, event->data_len);
                controller->handleMessage(topic, payload);
            }
            break;

        default: {

            if(event_id != 5){
                ESP_LOGI(TAG, "MQTT event ID: %" PRId32, event_id);
            }
        
            break;
        }
    }
}

void WifiMqttController::handleMessage(const std::string& topic, const std::string& payload) {
    //ESP_LOGI(TAG, "MQTT: %s   %s", topic.c_str(), payload.c_str());
    auto* msg = new MqttMsg{topic, payload};
    if (xQueueSend(msgQueue, &msg, 0) != pdPASS) {
        ESP_LOGW(TAG, "MQTT queue full, dropping message: %s", topic.c_str());
        delete msg; // evita leak si no pudo encolar
    }
}

void WifiMqttController::WorkerTask(void* arg) {
    auto* self = static_cast<WifiMqttController*>(arg);
    for (;;) {
        MqttMsg* raw = nullptr;
        if (xQueueReceive(self->msgQueue, &raw, portMAX_DELAY) == pdPASS && raw) {
            std::unique_ptr<MqttMsg> msg(raw);
            self->processInWorker(msg->topic, msg->payload);
        }
    }
}

void WifiMqttController::processInWorker(const std::string& topic,
                                         const std::string& payload) {
    
    ESP_LOGI(TAG, "MQTT: %s   %s", topic.c_str(), payload.c_str());
    
    const auto subTopics = StringUtil::split(topic, "/");
    if (subTopics.empty()) {
        ESP_LOGW(TAG, "Bad topic: '%s'", topic.c_str());
        return;
    }

    const std::string& taskType = subTopics.back();
    int taskTypeUid = -1;
    auto res = std::from_chars(taskType.data(), taskType.data() + taskType.size(), taskTypeUid);
    if (res.ec != std::errc()) {
        ESP_LOGE(TAG, "Non-numeric task type: %s", taskType.c_str());
        return;
    }

    auto it  = topicTaskMap.find(topic);
    auto it2 = topicTaskStateInfoSyncMap.find(topic);

    if (it != topicTaskMap.end()) {
        TaskController::instance().processMessageTaskData(taskTypeUid, payload, payload.length());
    } else if (it2 != topicTaskStateInfoSyncMap.end()) {
        TaskController::instance().processMessageTaskStateInfoSync(taskTypeUid, payload, payload.length());
    } else {
        ESP_LOGW(TAG, "No handler for topic: %s", topic.c_str());
    }
}