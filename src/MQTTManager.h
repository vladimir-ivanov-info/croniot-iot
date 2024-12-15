#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include "WiFiClient.h"

#include "Tasks/TaskBase.h"

#include <ArduinoJson.h>

#include "HttpController.h"

#include <functional>

#include "Storage.h"
#include "NetworkManager.h"

#include "Tasks/TaskController.h"

#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>

class MQTTManager{

    public:

        volatile bool initialized = false;

        MQTTManager() {
              mutex = xSemaphoreCreateMutex();
        }

        static MQTTManager & instance() {
            static MQTTManager * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new MQTTManager();
            }
            return *_instance;
        }

        String topic_outcoming = "";
        String topic_incoming = "";

        void init();
        void uninit();

        void publish(String topic, String message);

        void registerCallback(String topic, TaskBase* taskInstance);

    private:
        SemaphoreHandle_t mutex; 

        WiFiClient espClient;

        AsyncMqttClient mqttClient;
        Ticker mqttReconnectTimer;
        Ticker wifiReconnectTimer;

        TaskHandle_t taskHandle = NULL;

        std::map<String, TaskBase*> topicTaskMap;
       
        void connectToMqtt();

        void onWifiDisconnect(/*const WiFiEventStationModeDisconnected& event*/) {
            Serial.println("Disconnected from Wi-Fi.");
            mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
            //wifiReconnectTimer.once(2, connectToWifi);
        }

        void onMqttConnect(bool sessionPresent);

        void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

        void onMqttSubscribe(uint16_t packetId, uint8_t qos);

        void onMqttUnsubscribe(uint16_t packetId) {
            Serial.println("Unsubscribe acknowledged.");
            Serial.print("  packetId: ");
            Serial.println(packetId);
        }

        void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

        void onMqttPublish(uint16_t packetId) {
            Serial.println("Publish acknowledged.");
            Serial.print("  packetId: ");
            Serial.println(packetId);
        }

        void reconnectMQTT();

};

#endif