#ifndef WIFIMQTTCONTROLLER_H
#define WIFIMQTTCONTROLLER_H

#include "WiFiClient.h"

#include "Tasks/TaskBase.h"

#include <ArduinoJson.h>

#include "network/http/HttpController.h"

#include <functional>

#include "Storage.h"
#include "network/NetworkManager.h"

#include "Tasks/TaskController.h"

#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>

class WifiMqttController : public MqttController {

    public:

        //volatile bool initialized = false;

        WifiMqttController() {
              mutex = xSemaphoreCreateMutex();
        }

        static WifiMqttController & instance() {
            static WifiMqttController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new WifiMqttController();
            }
            return *_instance;
        }

        String topic_outcoming = "";
        String topic_incoming = "";

        bool init() override;
        void uninit();

        Result publish(String topic, String message) override;
        void registerCallback(String topic, TaskBase* taskInstance) override;

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