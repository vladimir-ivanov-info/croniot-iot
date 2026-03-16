#ifndef COMMONSETUP_H
#define COMMONSETUP_H

#include "network/mqtt/MqttProvider.h"
#include "network/NetworkManager.h"
#include "Tasks/TaskController.h"
#include "AuthenticationController.h"
#include "CurrentDateTimeController.h"
#include "network/http/HttpProvider.h"
#include "network/http/HttpController.h"
#include "esp_task_wdt.h"
#include "network/connection_provider/NetworkConnectionProvider.h"
#include "network/connection_provider/NetworkConnectionController.h"
#include "network/connection_provider/NetworkConnectionControllerBase.h"
#include "UserCredentials.h"
#include "Storage.h"

class CommonSetup {
public:
    static CommonSetup& instance() {
        static CommonSetup _instance;
        return _instance;
    }

    bool setup(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController) {
        return setupImpl(userCredentials, networkConnectionController);
    }

    UserCredentials userCredentials;

private:
    void authenticateWithServer(NetworkConnectionControllerBase* networkConnectionController);
    //void handleMqtt();

     

    static void authenticateWithServerTask(void* pvParameters);
    TaskHandle_t authenticateWithServerTaskTaskHandle;


    bool setupImpl(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController);
};

#endif