#include "WifiNetworkConnectionController.h"

bool WifiNetworkConnectionController::init(){
    //After this step, now we can finally connect to WiFi and to the server:
    setWifiConnected(false);

    Serial.begin(115200);
    while (!Serial) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    setupWifi();
    analogReadResolution(12); //TODO remove?

    esp_task_wdt_reset(); //TODO remove?

    return true; //TODO?
}

void WifiNetworkConnectionController::setupWifi(){
    Serial.println();
    WiFi.mode(WIFI_STA);
    //    WiFi.setSleep(false); // Desactiva la suspensión de wifi en modo STA para mejorar la velocidad de respuesta
  
    WiFi.onEvent(WifiNetworkConnectionController::wifiEventHandler);
  
    String wifiSsid = NetworkManager::instance().wifiSsid;
    String wifiPassword = NetworkManager::instance().wifiPassword;
  
    WiFi.begin(wifiSsid, wifiPassword);
  }
  
  void WifiNetworkConnectionController::wifiEventHandler(WiFiEvent_t event) {
    WifiNetworkConnectionController &instance = WifiNetworkConnectionController::instance();  // Get the singleton instance
  
      WiFiClient  espClient;

      switch (event) {
          case SYSTEM_EVENT_STA_GOT_IP:
                NetworkManager::instance().setConnectedToWifi(true);
                Serial.println("Connected to WiFi");
                Serial.print("IP Address:");
                Serial.println(WiFi.localIP());
  
                CurrentDateTimeController::instance().run();
                instance.setWifiConnected(true);

                xTaskCreatePinnedToCore(
                  mqttTask,         // Task function
                  "MQTT Task",      // Name of task
                  8192,             // Stack size (adjust if needed)
                  &instance,        // Task input parameter
                  2,                // Priority (2 is higher than the default of 1)
                  &instance.mqttTaskHandle,  // Task handle
                  1                 // Core to run the task on (1 = core 1 on dual-core ESP32)
                );

              break;
          case SYSTEM_EVENT_STA_DISCONNECTED:
              NetworkManager::instance().setConnectedToWifi(false);
              Serial.println("WiFi disconnected");
              instance.setWifiConnected(false);
              // WiFi.begin(wifiSsid, wifiPassword);
              break;
          default:
              break;
      }
  }
  
  void WifiNetworkConnectionController::setWifiConnected(bool connected){
  
    WifiNetworkConnectionController &instance = WifiNetworkConnectionController::instance();  // Get the singleton instance
  
    pinMode(instance.pinWifiConnected, OUTPUT);
    if(connected){
      digitalWrite(instance.pinWifiConnected, LOW);
      instance.wifiConnected = true;
    } else {
      digitalWrite(instance.pinWifiConnected, HIGH);
      instance.wifiConnected = false;
    }
  }
  
  void WifiNetworkConnectionController::mqttTask(void *pvParameters) {
  
    WifiNetworkConnectionController *instance = static_cast<WifiNetworkConnectionController *>(pvParameters);
    while (true) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      instance->handleMqtt();
      esp_task_wdt_reset();
    }
  }
  
  void WifiNetworkConnectionController::loop(){ //TODO remove?
  }
  
  //TODO delegate to MQTTController
  void WifiNetworkConnectionController::handleMqtt(){
    if(wifiConnected){
      if(!authenticationControllerInitialized){
        AuthenticationController::instance().init();
        authenticationControllerInitialized = true;
      }
      
      if(MqttProvider::get()->initialized){
          if(!taskControllerInitialized){
            TaskController::instance().init();
            taskControllerInitialized = true;
          }
        }
    } else {
      //TODO uninitialize the controllers (AuthenticationController, etc.)
    }
  }

  bool WifiNetworkConnectionController::connectedToNetwork(){
    return true; //TODO
  }