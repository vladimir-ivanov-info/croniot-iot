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

// Delayed reconnect (no blocking inside the WiFi event handler)
#include "esp_timer.h"

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

    // Reconnect-with-backoff state (see wifiEventHandler). A disconnect never
    // aborts the device; it schedules esp_wifi_connect() again after a delay
    // that grows with consecutive failures, so a wrong password or a missing
    // AP doesn't turn into a tight retry loop.
    int reconnectAttempt = 0;
    esp_timer_handle_t reconnectTimer = nullptr;
    static void reconnectTimerCallback(void* arg);

    static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data);
    void setWifiConnected(bool connected);

    void handleMqtt();
    static void mqttTask(void* pvParameters);
    TaskHandle_t mqttTaskHandle = nullptr;

};

#endif // WIFINETWORKCONNECTIONCONTROLLER_H
