#ifndef WIFINETWORKCONNECTIONCONTROLLER_H
#define WIFINETWORKCONNECTIONCONTROLLER_H

#include "NetworkConnectionController.h"
#include "NetworkManager.h"
#include "AuthenticationController.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// GPIO
#include "driver/gpio.h"

// Wi-Fi & Eventos
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"

// Watchdog
#include "esp_task_wdt.h"

// NVS & NETIF
#include "esp_netif.h"
#include "nvs_flash.h"

#include "ConnectionTypes.h"

static const char* TAG_WIFI = "WIFI_CTRL";

class WifiNetworkConnectionController : public NetworkConnectionController<WifiNetworkConnectionController> {
public:
    static WifiNetworkConnectionController& instance() {
        static WifiNetworkConnectionController inst;
        return inst;
    }

    bool init(connection::WifiConnectedCallback wifiConnectedCallback) override;
    bool connectedToNetwork() override;
    virtual ~WifiNetworkConnectionController() = default;

    WifiNetworkConnectionController() = default; //TODO poner private



private:
    

    bool taskCreated = false;
    gpio_num_t ledPin = GPIO_NUM_13;
    volatile bool wifiConnected = false;
    volatile bool authInitDone = false;
    bool taskCtrlInitDone = false;

    static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data);
    void setWifiConnected(bool connected);

    void handleMqtt();
    static void mqttTask(void* pvParameters);
    TaskHandle_t mqttTaskHandle = nullptr;

};

#endif // WIFINETWORKCONNECTIONCONTROLLER_H
