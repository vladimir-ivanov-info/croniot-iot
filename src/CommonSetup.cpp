#include "CommonSetup.h"

#include <WiFi.h>

void CommonSetup::setup(){

  setWifiConnected(false);

  Serial.begin(115200);
  while (!Serial) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  setupWifi();

  analogReadResolution(12);
}

void CommonSetup::setupWifi(){
  Serial.println();
  WiFi.mode(WIFI_STA);
      //    WiFi.setSleep(false); // Desactiva la suspensión de wifi en modo STA para mejorar la velocidad de respuesta

  WiFi.onEvent(CommonSetup::wifiEventHandler);

  String wifiSsid = NetworkManager::instance().wifiSsid;
  String wifiPassword = NetworkManager::instance().wifiPassword;

  WiFi.begin(wifiSsid, wifiPassword);
}

void CommonSetup::wifiEventHandler(WiFiEvent_t event) {
  CommonSetup &instance = CommonSetup::instance();  // Get the singleton instance

    WiFiClient  espClient;
    PubSubClient client(espClient);
   
    switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
              NetworkManager::instance().setConnectedToWifi(true);
              Serial.println("Connected to WiFi");
              Serial.print("IP Address:");
              Serial.println(WiFi.localIP());
              instance.setWifiConnected(true);
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

void CommonSetup::setWifiConnected(bool connected){

  CommonSetup &instance = CommonSetup::instance();  // Get the singleton instance

  pinMode(instance.pinWifiConnected, OUTPUT);
  if(connected){
    digitalWrite(instance.pinWifiConnected, LOW);
    instance.wifiConnected = true;
  } else {
    digitalWrite(instance.pinWifiConnected, HIGH);
    instance.wifiConnected = false;
  }
}

void CommonSetup::mqttTask(void *pvParameters) {

  CommonSetup *instance = static_cast<CommonSetup *>(pvParameters);
  while (true) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    instance->handleMqtt();
  }
}

void CommonSetup::loop(){
  // Create a FreeRTOS task for the MQTT loop with higher priority
  if(!taskCreated){
    taskCreated = true;

    xTaskCreatePinnedToCore(
      mqttTask,         // Task function
      "MQTT Task",      // Name of task
      8192,             // Stack size (adjust if needed)
      this,             // Task input parameter
      2,                // Priority (2 is higher than the default of 1)
      &mqttTaskHandle,  // Task handle
      1                 // Core to run the task on (1 = core 1 on dual-core ESP32)
    );

  }
}

void CommonSetup::handleMqtt(){
  if(wifiConnected){
    if(!authenticationControllerInitialized){
      AuthenticationController::instance().init();
      authenticationControllerInitialized = true;
    }
    
    if(MQTTManager::instance().initialized){
        MQTTManager::instance().getClient()->loop();
        if(!taskControllerInitialized){
          TaskController::instance().init();
          taskControllerInitialized = true;
        }
      }

  } else {
    //TODO uninitialize the controllers (AuthenticationController, etc.)
  }
}