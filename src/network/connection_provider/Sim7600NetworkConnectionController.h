#ifndef SIM7600NETWORKCONNECTIONCONTROLLER_H
#define SIM7600NETWORKCONNECTIONCONTROLLER_H

#include <Arduino.h>
#include "NetworkConnectionController.h"
//#include "esp_task_wdt.h"

#include "../NetworkManager.h"
#include "../../AuthenticationController.h"

#include <freertos/FreeRTOS.h>

#include "network/sim7600/Sim7600.h"

#include "network/http/Sim7600HttpController.h"
#include "network/http/HttpProvider.h"


class Sim7600NetworkConnectionController : public NetworkConnectionController<Sim7600NetworkConnectionController> {
    
    public:
        bool init();
        bool connectedToNetwork();
        
        virtual ~Sim7600NetworkConnectionController() = default;

    private:

        static void mqttTask(void *pvParameters);
        TaskHandle_t mqttTaskHandle;


        bool taskCreated = false;

        int pinWifiConnected = 13; //for easily and visually knowing the current WiFi connection state. I used an LED: light = error, no light = connected
        volatile bool wifiConnected = false;
        volatile bool authenticationControllerInitialized = false;
        bool taskControllerInitialized = false;
        
        static void setupWifi();
        static void wifiEventHandler(WiFiEvent_t event);
        void setWifiConnected(bool connected);

        void handleMqtt();
};

#endif