#include "CommonSetup.h"
#include "esp_log.h"

static const char* TAG = "CommonSetup";

bool CommonSetup::setupImpl(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController) {
    UserCredentials credentialsInMemory = Storage::instance().readUserCredentials();

    ESP_LOGI(TAG, "\nCredentials in memory: [%s,%s,%s,%s,%s,%s]",
             credentialsInMemory.accountEmail.c_str(),
             credentialsInMemory.accountUuid.c_str(),
             credentialsInMemory.accountPassword.c_str(),
             credentialsInMemory.deviceUuid.c_str(),
             credentialsInMemory.deviceName.c_str(),
             credentialsInMemory.deviceDescription.c_str());

    ESP_LOGI(TAG, "\nCredentials defined by user: [%s,%s,%s,%s,%s,%s]",
             userCredentials.accountEmail.c_str(),
             userCredentials.accountUuid.c_str(),
             userCredentials.accountPassword.c_str(),
             userCredentials.deviceUuid.c_str(),
             userCredentials.deviceName.c_str(),
             userCredentials.deviceDescription.c_str());

    if (credentialsInMemory.accountEmail != userCredentials.accountEmail ||
        credentialsInMemory.accountUuid != userCredentials.accountUuid ||
        credentialsInMemory.accountPassword != userCredentials.accountPassword ||
        credentialsInMemory.deviceUuid != userCredentials.deviceUuid ||
        credentialsInMemory.deviceName != userCredentials.deviceName) {

        ESP_LOGI(TAG, "Storing new credentials");

        UserCredentials newCredentials = UserCredentials(
            userCredentials.accountEmail, userCredentials.accountUuid,
            userCredentials.accountPassword, userCredentials.deviceUuid,
            "", userCredentials.deviceName, userCredentials.deviceDescription);

        Storage::instance().saveUserCredentials(newCredentials);
        vTaskDelay(pdMS_TO_TICKS(500));
    }



    //static bool wifi_connected = false;


    connection::WifiConnectedCallback callback = [this](const std::string& ssid) {
        ESP_LOGI("CommonSetup", ">>>>>>>>>>Connected to: %s", ssid.c_str());

      //  wifi_connected = true;  // Solo flag, nada más

        xTaskCreate(
            CommonSetup::authenticateWithServerTask,
            "authenticateWithServerTask",
            16384,
            this,
            1,
            &this->authenticateWithServerTaskTaskHandle
            //&authenticateWithServerTaskTaskHandle
        );

       // bool authenticated = AuthenticationController::instance().init();
       // ESP_LOGI(TAG, "###AUTHENTICATED WITH SERVER: %s", authenticated ? "true" : "false");

        /*if (authenticated) {
           // ESP_LOGI(TAG, "###AUTHENTICATED WITH SERVER");

            bool mqttInitialized = MqttProvider::get()->init();
            if (mqttInitialized) {
                SensorsController::instance().init();
            } else {
                ESP_LOGE(TAG, "Could not initialize MQTT...");
            }
        }*/
    };

    //authenticateWithServer(networkConnectionController);
    bool networkConnectionProviderInitialized = NetworkConnectionProvider::init(networkConnectionController, callback); //TODO no hace falta devolver bool, sino hay un callback



   /* while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    bool authenticated = AuthenticationController::instance().init();
    ESP_LOGI(TAG, "###AUTHENTICATED WITH SERVER: %s", authenticated ? "true" : "false");

    if (authenticated) {
        bool mqttInitialized = MqttProvider::get()->init();
        if (mqttInitialized) {
            SensorsController::instance().init();
        } else {
            ESP_LOGE(TAG, "Could not initialize MQTT...");
        }
    }*/

    
    return true;
}

void CommonSetup::authenticateWithServer(NetworkConnectionControllerBase* networkConnectionController) {
   /* bool networkConnectionProviderInitialized = NetworkConnectionProvider::init(networkConnectionController);

    ESP_LOGI(TAG, "###ConnectedToNetwork: %s", NetworkConnectionProvider::get()->connectedToNetwork() ? "true" : "false");


    while(!NetworkConnectionProvider::get()->connectedToNetwork()){
        ESP_LOGI(TAG, "###ConnectedToNetwork: %s", NetworkConnectionProvider::get()->connectedToNetwork() ? "true" : "false");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }*/

   // if (networkConnectionProviderInitialized) {
      /*  bool authenticated = AuthenticationController::instance().init();
        ESP_LOGI(TAG, "###AUTHENTICATED WITH SERVER: %s", authenticated ? "true" : "false");

        if (authenticated) {
           // ESP_LOGI(TAG, "###AUTHENTICATED WITH SERVER");

            bool mqttInitialized = MqttProvider::get()->init();
            if (mqttInitialized) {
                SensorsController::instance().init();
            } else {
                ESP_LOGE(TAG, "Could not initialize MQTT...");
            }
        }*/
   // } else {
  //      ESP_LOGE(TAG, "Could not initialize network connection provider... Is server on?");
   // }
}


void CommonSetup::authenticateWithServerTask(void* pvParameters) {
    CommonSetup* self = static_cast<CommonSetup*>(pvParameters);


    bool authenticated = AuthenticationController::instance().init();
    ESP_LOGI(TAG, "\n\n\n###AUTHENTICATED WITH SERVER: %s", authenticated ? "true" : "false");

    CurrentDateTimeController::instance().run();

    if (authenticated) {
        bool mqttInitialized = MqttProvider::get()->init();
        if (mqttInitialized) {
            SensorsController::instance().init();
            TaskController::instance().init();
        } else {
            ESP_LOGE(TAG, "Could not initialize MQTT...");
        }
    }

    while(true){
        //ESP_LOGI(TAG, "### CommonSetup authenticateWithServerTask waiting...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}