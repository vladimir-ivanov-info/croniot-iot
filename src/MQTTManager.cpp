#include "MQTTManager.h"

#include "stdio.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

void MQTTManager::callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    Serial.print("MQTT message arrived on topic: "); Serial.println(topicStr);

    if (instance().topicTaskMap.find(topicStr) != instance().topicTaskMap.end()) {
        Serial.print("MQTT topic correct: ["); Serial.print(topic); Serial.println("]");
        // Allocate memory for the payload
        byte* payloadCopy = (byte*)pvPortMalloc(length);
        if (payloadCopy == NULL) {
            Serial.println("Failed to allocate memory for payload.");
            return;
        }
        memcpy(payloadCopy, payload, length);

        // Enqueue the message to the appropriate task instance
        instance().topicTaskMap[topicStr]->enqueueMessage(topicStr, payloadCopy, length);
    } else {
        Serial.print("Received message on unknown topic: ");
        Serial.println(topic);
    }
}

void MQTTManager::callbackTaskFunction(void* pvParameters) {
    CallbackTaskParams* params = static_cast<CallbackTaskParams*>(pvParameters);
    params->callback(params->topic, params->payload, params->length);

    // Free allocated memory
    vPortFree(params->payload);
    delete params;

    vTaskDelete(NULL); // Delete the task after executing the callback
}


void MQTTManager::init(){

    ServerData serverData = Storage::instance().getServerData();
    String serverAddress = serverData.serverAddress;

    uint16_t serverMqttPort = NetworkManager::instance().serverMqttPort;
    client.setServer(serverAddress.c_str(), serverMqttPort);

    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect("ESP32Client")) {
            //client->setKeepAlive(60*60); // Set keep-alive interval to 15 seconds (adjust as needed)
            Serial.println("Connected to MQTT broker.");
            client.setCallback([this](char* topic, byte* payload, unsigned int length) {
                this->callback(topic, payload, length);
            });
            initialized = true;
            break;
        } else {
            Serial.print("MQTT connection failed, rc="); Serial.print(client.state()); Serial.println(" Retrying in 1 second...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

void MQTTManager::registerCallback(String topic, TaskBase* taskInstance) {
    topicTaskMap[topic] = taskInstance;
    bool subscriptionSuccess = client.subscribe(topic.c_str(), MQTTQOS0); //MQTTQOS2 : QoS = exactly once
    if (subscriptionSuccess) {
        Serial.print("MQTT callback registered: ["); Serial.print(topic); Serial.println("]");
    } else {
        Serial.print("Failed to subscribe to topic: ["); Serial.print(topic); Serial.print("]");
    }
        
}

void MQTTManager::uninit(){
    client.disconnect();
}

void MQTTManager::publish(String topic, String message){
    try{
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            if(client.connected()){
                boolean succes = client.publish(topic.c_str(), message.c_str());
            } else {
                Serial.println("---------->>>Mqtt client not connected, trying to reconnect");

                //TODO check if we need to assign the server and port or the mqtt client already remembers them
                ServerData serverData = Storage::instance().getServerData();
                String serverAddress = serverData.serverAddress;

                uint16_t serverMqttPort = NetworkManager::instance().serverMqttPort;
                client.setServer(serverAddress.c_str(), serverMqttPort);

                client.connect("ESP32Client");
            }

            xSemaphoreGive(mutex); // Release the mutex
        }
    } catch(const std::exception& e){
        Serial.println(">>>>>>>>>Exception in MQTTManager::publish");
        Serial.println("<<<<<<<<<<");
        xSemaphoreGive(mutex); // Release the mutex
    }
}