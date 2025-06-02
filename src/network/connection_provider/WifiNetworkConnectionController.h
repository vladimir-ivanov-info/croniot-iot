#ifndef WIFINETWORKCONNECTIONCONTROLLER_H
#define WIFINETWORKCONNECTIONCONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include "NetworkConnectionController.h"
#include "esp_task_wdt.h"

#include "../NetworkManager.h"
#include "../../AuthenticationController.h"

#include <freertos/FreeRTOS.h>

#include "WifiNetworkConnectionController.h"


class WifiNetworkConnectionController : public NetworkConnectionController<WifiNetworkConnectionController> {
    
    public:
        static WifiNetworkConnectionController& instance() {
            static WifiNetworkConnectionController instance;
            return instance;
        }
        
        bool init();
        bool connectedToNetwork();
        virtual ~WifiNetworkConnectionController() = default;

    private:
        bool taskCreated = false;


        int pinWifiConnected = 13; //for easily and visually knowing the current WiFi connection state. I used an LED: light = error, no light = connected
        volatile bool wifiConnected = false;
        volatile bool authenticationControllerInitialized = false;
        bool taskControllerInitialized = false;
        
        static void setupWifi();
        static void wifiEventHandler(WiFiEvent_t event);
        void setWifiConnected(bool connected);

        void handleMqtt();

        static void mqttTask(void *pvParameters);
        TaskHandle_t mqttTaskHandle;

        void loop();
};

#endif