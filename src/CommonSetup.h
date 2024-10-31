#ifndef COMMONSETUP_H
#define COMMONSETUP_H

#include <WiFi.h>

#include "MQTTManager.h"
#include "NetworkManager.h"
#include "Tasks/TaskController.h"
#include "AuthenticationController.h"
#include "CurrentDateTimeController.h"

class CommonSetup{

    public:
        static CommonSetup & instance() {
            static  CommonSetup * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new CommonSetup();
            }
            return *_instance;
        }

        void setup(UserCredentials userCredentials);
        void loop();


    private:
        int pinWifiConnected = 13; //for easily and visually knowing the current WiFi connection state. I used an LED: light = error, no light = connected
        volatile bool wifiConnected = false;
        volatile bool authenticationControllerInitialized = false;
        bool taskControllerInitialized = false;
        
        static void setupWifi();
        static void wifiEventHandler(WiFiEvent_t event);
        void setWifiConnected(bool connected);

        void handleMqtt();

        bool taskCreated = false;

        static void mqttTask(void *pvParameters);
        TaskHandle_t mqttTaskHandle;

        UserCredentials userCredentials;

};

#endif