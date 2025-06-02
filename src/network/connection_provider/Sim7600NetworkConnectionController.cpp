#include "Sim7600NetworkConnectionController.h"

bool Sim7600NetworkConnectionController::init(){
    bool sim7600Initialized = Sim7600::instance().init();

    HttpProvider::set(new Sim7600HttpController());

    return sim7600Initialized;
}
    
void Sim7600NetworkConnectionController::mqttTask(void *pvParameters) {

  Sim7600NetworkConnectionController *instance = static_cast<Sim7600NetworkConnectionController *>(pvParameters);
  instance->taskCreated = false;

  while (true) {
      if(!instance->taskCreated){
          instance->taskCreated = true;
          vTaskDelay(500 / portTICK_PERIOD_MS);
          //TODO instance->handleMqtt();
          Sim7600::instance().init();
      } else {
          esp_task_wdt_reset();   // Reseteas el Watchdog
          vTaskDelay(3000 / portTICK_PERIOD_MS);
          //esp_task_wdt_reset();
      }

      vTaskDelay(3000 / portTICK_PERIOD_MS);
      esp_task_wdt_reset();
  }
}

bool Sim7600NetworkConnectionController::connectedToNetwork(){
  return true; //TODO
}