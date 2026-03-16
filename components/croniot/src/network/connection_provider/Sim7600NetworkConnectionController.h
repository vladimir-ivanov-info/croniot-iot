#ifndef SIM7600NETWORKCONNECTIONCONTROLLER_H
#define SIM7600NETWORKCONNECTIONCONTROLLER_H

#include "NetworkConnectionController.h"
#include "NetworkManager.h"
#include "AuthenticationController.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Watchdog (opcional)
#include "esp_task_wdt.h"

// ESP-IDF Wi-Fi
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"

// HTTP over SIM7600
#include "network/sim7600/Sim7600.h"
#include "network/http/Sim7600HttpController.h"
#include "network/http/HttpProvider.h"

//static const char* TAG = "SIM7600_NET";

class Sim7600NetworkConnectionController : public NetworkConnectionController<Sim7600NetworkConnectionController> {
public:
    bool init(connection::WifiConnectedCallback wifiConnectedCallback) override;
    bool connectedToNetwork() override;
    virtual ~Sim7600NetworkConnectionController() = default;

private:
    static void mqttTask(void *pvParameters);
    TaskHandle_t mqttTaskHandle = nullptr;

    bool taskCreated = false;

    // si quieres indicar estado con un LED
    static constexpr gpio_num_t LED_PIN = GPIO_NUM_13;
    volatile bool networkUp = false;

    //void setNetworkUp(bool up);

    void handleMqtt();
};

#endif // SIM7600NETWORKCONNECTIONCONTROLLER_H
