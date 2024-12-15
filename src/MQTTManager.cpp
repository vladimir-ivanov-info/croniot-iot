#include "MQTTManager.h"

void MQTTManager::init(){
    mqttClient.onConnect([this](bool sessionPresent) {
        this->onMqttConnect(sessionPresent);
    });

//mqttClient.onDisconnect(onMqttDisconnect);

    mqttClient.onSubscribe([this](uint16_t packetId, uint8_t qos) {
        this->onMqttSubscribe(packetId, qos);
    });

//mqttClient.onUnsubscribe(onMqttUnsubscribe);
    //mqttClient.onMessage(onMqttMessage);


    mqttClient.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        this->onMqttMessage(topic, payload, properties, len, index, total);
    });

//mqttClient.onPublish(onMqttPublish);

    //ServerData serverData = Storage::instance().getServerData();

    uint16_t serverMqttPort = NetworkManager::instance().serverMqttPort;
    mqttClient.setServer("51.77.195.204", serverMqttPort);

    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void MQTTManager::registerCallback(String topic, TaskBase* taskInstance) {
    topicTaskMap[topic] = taskInstance;
    
    uint16_t pcketId = mqttClient.subscribe(topic.c_str(), 2);
    if (pcketId > 0) {
        Serial.print("MQTT callback registered: ["); Serial.print(topic); Serial.println("]");
    } else {
        Serial.print("Failed to subscribe to topic: ["); Serial.print(topic); Serial.print("]");
    }
}

void MQTTManager::uninit(){
    //client.disconnect();
}

void MQTTManager::publish(String topic, String message){
    try{
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            if(mqttClient.connected()){
                //uint16_t packetIdPub2 = mqttClient.publish(topic.c_str(), 2, true, message.c_str());
                    uint16_t packetIdPub2 = mqttClient.publish(topic.c_str(), 2, false, message.c_str());
                        if (packetIdPub2 == 0) {
                            Serial.println("Failed to publish message. Packet ID is 0.");
                        }
            } else {
                mqttClient.disconnect();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                reconnectMQTT();
            }
            xSemaphoreGive(mutex); // Release the mutex
        }
    } catch(const std::exception& e){
        Serial.println(">>>>>>>>>Exception in MQTTManager::publish");
        Serial.println("<<<<<<<<<<");
        xSemaphoreGive(mutex); // Release the mutex
    }
}

void MQTTManager::reconnectMQTT() {
    if (!mqttClient.connected()) {
        Serial.println("Attempting MQTT reconnect...");
        mqttClient.connect();
    }
}

void MQTTManager::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("Disconnected from WiFi and MQTT.");

    /*if (WiFi.isConnected()) {
        mqttReconnectTimer.once(2, connectToMqtt);
    }*/
}

void MQTTManager::connectToMqtt() {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void MQTTManager::onMqttConnect(bool sessionPresent) {
    initialized = true;
    Serial.println("MQTT connected.");
}

void MQTTManager::onMqttSubscribe(uint16_t packetId, uint8_t qos) {
    /*Serial.println("Subscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
    Serial.print("  qos: ");
    Serial.println(qos);*/
}

void MQTTManager::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    String topicStr = String(topic);
    Serial.print("MQTT message arrived on topic: "); Serial.println(topicStr);

    int subTopicsCount = 0;
    String *subTopics = StringUtil::split(topicStr, "/", subTopicsCount);
    
    String taskType = subTopics[subTopicsCount - 1];
    Serial.print("Task type: "); Serial.println(taskType);
    
    TaskController::instance().processMessage(taskType.toInt(), String(payload), len);
}