// #include "WifiMqttController.h"

// bool WifiMqttController::init(){
//     mqttClient.onConnect([this](bool sessionPresent) {
//         this->onMqttConnect(sessionPresent);
//     });

// //mqttClient.onDisconnect(onMqttDisconnect);

//     mqttClient.onSubscribe([this](uint16_t packetId, uint8_t qos) {
//         this->onMqttSubscribe(packetId, qos);
//     });

// //mqttClient.onUnsubscribe(onMqttUnsubscribe);
//     //mqttClient.onMessage(onMqttMessage);


//     mqttClient.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
//         this->onMqttMessage(topic, payload, properties, len, index, total);
//     });

// //mqttClient.onPublish(onMqttPublish);

//     //ServerData serverData = Storage::instance().getServerData();

//     uint16_t serverMqttPort = NetworkManager::instance().serverMqttPort;
//     mqttClient.setServer("51.77.195.204", serverMqttPort);

//     Serial.println("Connecting to MQTT...");
//     mqttClient.connect();

//     return true;
// }

// void WifiMqttController::registerCallback(String topic, TaskBase* taskInstance) {
//     topicTaskMap[topic] = taskInstance;
    
//     uint16_t pcketId = mqttClient.subscribe(topic.c_str(), 2);
//     if (pcketId > 0) {
//         Serial.print("MQTT callback registered: ["); Serial.print(topic); Serial.println("]");
//     } else {
//         Serial.print("Failed to subscribe to topic: ["); Serial.print(topic); Serial.print("]");
//     }
// }

// void WifiMqttController::uninit(){
//     //client.disconnect();
// }

// Result WifiMqttController::publish(String topic, String message){
//     try{
//         if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
//             if(mqttClient.connected()){
//                     uint16_t packetIdPub2 = mqttClient.publish(topic.c_str(), 2, false, message.c_str());
//                         if (packetIdPub2 == 0) {
//                             Serial.println("Failed to publish message. Packet ID is 0.");
//                         }
//             } else {
//                 mqttClient.disconnect();
//                 vTaskDelay(1000 / portTICK_PERIOD_MS);
//                 reconnectMQTT();
//             }
//             xSemaphoreGive(mutex); // Release the mutex
//         }
//     } catch(const std::exception& e){
//         Serial.println(">>>>>>>>>Exception in MQTTManager::publish");
//         Serial.println("<<<<<<<<<<");
//         xSemaphoreGive(mutex); // Release the mutex
//     }

//     return Result(true, "wqeqweqwe");
// }

// void WifiMqttController::reconnectMQTT() {
//     if (!mqttClient.connected()) {
//         Serial.println("Attempting MQTT reconnect...");
//         mqttClient.connect();
//     }
// }

// void WifiMqttController::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
//     Serial.println("Disconnected from WiFi and MQTT.");

//     /*if (WiFi.isConnected()) {
//         mqttReconnectTimer.once(2, connectToMqtt);
//     }*/
// }

// void WifiMqttController::connectToMqtt() {
//     Serial.println("Connecting to MQTT...");
//     mqttClient.connect();
// }

// void WifiMqttController::onMqttConnect(bool sessionPresent) {
//     initialized = true;
//     Serial.println("MQTT connected.");
// }

// void WifiMqttController::onMqttSubscribe(uint16_t packetId, uint8_t qos) {
//     /*Serial.println("Subscribe acknowledged.");
//     Serial.print("  packetId: ");
//     Serial.println(packetId);
//     Serial.print("  qos: ");
//     Serial.println(qos);*/
// }

// void WifiMqttController::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
//     String topicStr = String(topic);
//     Serial.print("MQTT message arrived on topic: "); Serial.println(topicStr);

//     int subTopicsCount = 0;
//     String *subTopics = StringUtil::split(topicStr, "/", subTopicsCount);
    
//     String taskType = subTopics[subTopicsCount - 1];
//     Serial.print("Task type: "); Serial.println(taskType);
    
//     TaskController::instance().processMessage(taskType.toInt(), String(payload), len);
// }


#include "esp_timer.h"
#include <inttypes.h>  // para PRId32


#include "WifiMqttController.h"
#include "network/NetworkManager.h"
#include "Tasks/TaskController.h"

#define TAG "WifiMqttController"

/*bool WifiMqttController::init() {
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
    }

    esp_mqtt_client_config_t config = {
        .uri = ("mqtt://" + NetworkManager::instance().serverAddress + ":" + std::to_string(NetworkManager::instance().serverMqttPort)).c_str()
    };

    mqttClient = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(mqttClient, ESP_EVENT_ANY_ID, &mqttEventHandler, this);

    ESP_LOGI(TAG, "Connecting to MQTT...");
    return esp_mqtt_client_start(mqttClient) == ESP_OK;
}*/

/*bool WifiMqttController::init() {
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
    }

    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = NetworkManager::instance().serverAddress.c_str();
    config.broker.address.port = NetworkManager::instance().serverMqttPort;

    mqttClient = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(mqttClient, ESP_EVENT_ANY_ID, &mqttEventHandler, this);

    ESP_LOGI(TAG, "Connecting to MQTT...");
    return esp_mqtt_client_start(mqttClient) == ESP_OK;
}*/

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

    uriStr = "mqtt://192.168.50.163:" + std::to_string(NetworkManager::instance().serverMqttPort); //TODO

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

   // if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        int msgId = esp_mqtt_client_publish(mqttClient, topic.c_str(), message.c_str(), 0, 1, 0);
        //ESP_LOGI(TAG, "MQTT msgId msgId msgId msgId...");
    //    xSemaphoreGive(mutex);
        if (msgId > 0) {
            //ESP_LOGI(TAG, "Published successfully...");
            return Result(true, "Published successfully");
        } else {
            ESP_LOGI(TAG, "MQTT state: %d", msgId);
        }
        //ESP_LOGI(TAG, "Published failed...");
        return Result(false, "Publish failed");
    //}

    //ESP_LOGI(TAG, "MQTT publish mutex timeout...");
    return Result(false, "MQTT publish mutex timeout");


      //  return Result(false, "Testing the code...");

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

/*
    //meter en cola y que otra tarea los vaya procesando

//     String topicStr = String(topic);
//     Serial.print("MQTT message arrived on topic: "); Serial.println(topicStr);

//     int subTopicsCount = 0;
     //String *subTopics = StringUtil::split(topicStr, "/", subTopicsCount);

     //std::vector<std::string> subTopics = StringUtil::split(topicStr, "/", subTopicsCount);
    const std::vector<std::string> subTopics = StringUtil::split(topic, "/");
    if (subTopics.empty()) {
        ESP_LOGW(TAG, "Received MQTT topic with no sub-topic: '%s'", topic.c_str());
        return;
    }

    const std::string& taskType = subTopics.back();
    ESP_LOGI(TAG, "Task type: %s", taskType.c_str());

     //std::string taskType = subTopics[subTopicsCount - 1];
//     Serial.print("Task type: "); Serial.println(taskType);
       // ESP_LOGI(TAG, "Task type: %s", taskType.c_str());

//     TaskController::instance().processMessage(taskType.toInt(), String(payload), len);


    ESP_LOGI(TAG, "Message received on topic: %s", topic.c_str());



    int taskTypeUid = -1;
    auto res = std::from_chars(taskType.data(),
                               taskType.data() + taskType.size(),
                               taskTypeUid);       // base 10 por defecto

    if (res.ec != std::errc()) {
        ESP_LOGE(TAG, "Task type no numérico o fuera de rango: %s",
                 taskType.c_str());
        return;
    }


    auto it = topicTaskMap.find(topic);
    auto itTaskStateInfoSyncMap = topicTaskStateInfoSyncMap.find(topic);
    
    if (it != topicTaskMap.end()) {
        // Puedes pasar payload a tu task directamente o procesarlo como string.
        //TaskController::instance().processMessage(tasTypeUid, payload, payload.length());
        TaskController::instance().processMessageTaskData(taskTypeUid, payload, payload.length());
    } else if(itTaskStateInfoSyncMap != topicTaskStateInfoSyncMap.end()){ //TODOOOOOOOOOOOOOOO
        TaskController::instance().processMessageTaskStateInfoSync(taskTypeUid, payload, payload.length());
    }
    else {
        ESP_LOGW(TAG, "No handler registered for topic: %s", topic.c_str());
    }
    */
}



void WifiMqttController::WorkerTask(void* arg) {
    auto* self = static_cast<WifiMqttController*>(arg);
    for (;;) {
        MqttMsg* msg = nullptr;
        if (xQueueReceive(self->msgQueue, &msg, portMAX_DELAY) == pdPASS && msg) {
            self->processInWorker(msg->topic, msg->payload);
            delete msg;
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