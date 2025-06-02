#ifndef COMMONSETUP_H
#define COMMONSETUP_H

#include <WiFi.h>

#include "network/mqtt/MqttProvider.h"
#include "network/NetworkManager.h"
#include "Tasks/TaskController.h"
#include "AuthenticationController.h"
#include "CurrentDateTimeController.h"

#include "network/http/HttpProvider.h"
#include "network/http/HttpController.h"

#include "network/mqtt/MqttProvider.h"

#include <memory>


#include "esp_task_wdt.h"

#include "network/connection_provider/NetworkConnectionProvider.h"
#include "network/connection_provider/NetworkConnectionController.h"

#include "AuthenticationController.h"

#include "network/connection_provider/NetworkConnectionControllerBase.h"

class CommonSetup {

    public:
        static CommonSetup & instance() {
            static  CommonSetup * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new CommonSetup();
            }
            return *_instance;
        }

        bool setup(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController) {
            setupImpl(userCredentials, networkConnectionController);
            return true; //TODO
        }

        UserCredentials userCredentials;


    private:
        void authenticateWithServer(NetworkConnectionControllerBase* networkConnectionController);

        void handleMqtt();

        static void authenticateWithServerTask(void *pvParameters);
        TaskHandle_t mqttTaskHandle;

        bool setupImpl(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController);

};

#endif