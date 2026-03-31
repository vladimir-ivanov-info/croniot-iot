#include "driver/gpio.h"
#include "Sim7600NetworkConnectionController.h"

static const char *TAG = "Sim7600NetworkConnectionController";

bool Sim7600NetworkConnectionController::init(connection::WifiConnectedCallback wifiConnectedCallback) { //TODO rename to NetworkConnectedCallback
    // Inicializa la SIM
    bool ok = Sim7600::instance().init();
    if (!ok) {
        ESP_LOGE(TAG, "SIM7600 init failed");
        return false;
    }

    // Configura el proveedor HTTP
    HttpProvider::set(new Sim7600HttpController());


    // Configura GPIO para LED si lo necesitas
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    xTaskCreate(&Sim7600NetworkConnectionController::mqttTask, "mqtt_task", 4096, this, configMAX_PRIORITIES - 1, &mqttTaskHandle);

    return true;
}

bool Sim7600NetworkConnectionController::connectedToNetwork() {
    //return networkUp; //TODO
    return true; //TODO
}
/*
void Sim7600NetworkConnectionController::setNetworkUp(bool up) {
    networkUp = up;
    gpio_set_level(LED_PIN, up ? 0 : 1);  // LED off si conectado, on si error
}*/

void Sim7600NetworkConnectionController::mqttTask(void *pvParameters) {
    auto instance = static_cast<Sim7600NetworkConnectionController*>(pvParameters);

    while (true) {
        if (!instance->taskCreated) {
            instance->taskCreated = true;
            vTaskDelay(pdMS_TO_TICKS(500));
            instance->handleMqtt();
        } else {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}

void Sim7600NetworkConnectionController::handleMqtt() {
    // TODO: aquí pones la lógica de tu MQTT sobre SIM7600
    ESP_LOGI(TAG, "handleMqtt()");
    // por ejemplo: Sim7600::instance().mqttStart();
}
