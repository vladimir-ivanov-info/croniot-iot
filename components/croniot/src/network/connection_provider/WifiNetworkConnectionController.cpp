#include <cstring>
#include "WifiNetworkConnectionController.h"

static const char* TAG = "WifiNetworkConnectionController";

bool WifiNetworkConnectionController::init(NetworkConnectionProvider::WifiConnectedCallback wifiConnectedCallback) {
    this->wifiConnectedCallback = wifiConnectedCallback;
    wifiConnected = false;

    ESP_LOGI(TAG, "Starting WiFi initialization...");

    // ✅ Reset completo de NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_LOGI(TAG, "NVS erased and reinitialized");
    }

    // ✅ Inicialización con reset forzado
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Intentar deinicializar WiFi si ya existe
    esp_err_t deinit_result = esp_wifi_deinit();
    if (deinit_result == ESP_OK) {
        ESP_LOGI(TAG, "Previous WiFi deinitialized");
    }
    vTaskDelay(pdMS_TO_TICKS(200));  // Delay para estabilización
    
    esp_netif_create_default_wifi_sta();

    // ✅ Configuración robusta con calibración forzada
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 1;           // Habilitar NVS para calibración
    cfg.nano_enable = 0;          // Desactivar nano formatting
    cfg.ampdu_rx_enable = 0;      // Desactivar AMPDU para señal débil
    cfg.ampdu_tx_enable = 0;      // Desactivar AMPDU TX también
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // PHY calibration data preserved across boots (faster startup)

    // ✅ Configuración de país antes de todo
    wifi_country_t country = {
        .cc = "ES",
        .schan = 1,
        .nchan = 13,
        .max_tx_power = 20,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_err_t country_result = esp_wifi_set_country(&country);
    ESP_LOGI(TAG, "Country set result: %s", esp_err_to_name(country_result));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &WifiNetworkConnectionController::wifiEventHandler,
        this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &WifiNetworkConnectionController::wifiEventHandler,
        this, nullptr));

    // ✅ Configuración WiFi optimizada para señal débil
    wifi_config_t wifi_cfg = {};
    auto& nm = NetworkManager::instance();
    strncpy((char*)wifi_cfg.sta.ssid, nm.wifiSsid.c_str(), sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, nm.wifiPassword.c_str(), sizeof(wifi_cfg.sta.password));
    
    // Configuración específica para señal débil
    wifi_cfg.sta.channel = 0;                           // Auto-scan
    wifi_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;   // Escanear todos los canales
    wifi_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL; // Conectar por señal más fuerte
    wifi_cfg.sta.threshold.rssi = -127;                 // Umbral muy bajo para señal débil
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;   // Cualquier modo de auth
    wifi_cfg.sta.bssid_set = false;                     // No fijar BSSID específico
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // ✅ Tiempo para estabilización del RF
    ESP_LOGI(TAG, "Waiting for RF stabilization...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ✅ Configuraciones de potencia después de start
    //POWER SAVE: esp_err_t ps_result = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_err_t ps_result = esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "Power save MIN_MODEM result: %s", esp_err_to_name(ps_result));
    
    esp_err_t power_result = esp_wifi_set_max_tx_power(84);
    ESP_LOGI(TAG, "Max TX power set result: %s", esp_err_to_name(power_result));
    
    // ✅ Múltiples intentos de configuración de protocolo
    for (int i = 0; i < 3; i++) {
        esp_err_t protocol_result = esp_wifi_set_protocol(WIFI_IF_STA, 
            WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        ESP_LOGI(TAG, "Protocol set attempt %d result: %s", i+1, esp_err_to_name(protocol_result));
        if (protocol_result == ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // ✅ Bandwidth para mejor alcance
    esp_err_t bw_result = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    ESP_LOGI(TAG, "Bandwidth set result: %s", esp_err_to_name(bw_result));

    // ✅ Verificación del estado antes de conectar
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "WiFi mode: %d", mode);

    wifi_config_t current_config;
    esp_wifi_get_config(WIFI_IF_STA, &current_config);
    ESP_LOGI(TAG, "WiFi config SSID: %s", current_config.sta.ssid);


    // ✅ Configuraciones adicionales para mantener potencia
//esp_wifi_set_storage(WIFI_STORAGE_RAM);  // No guardar en flash
//esp_wifi_set_ps(WIFI_PS_NONE);           // Confirmar power saving OFF

    ESP_LOGI(TAG_WIFI, "Wi-Fi started, connecting to '%s'", nm.wifiSsid.c_str());
    ESP_ERROR_CHECK(esp_wifi_connect());

    return true;
}

void WifiNetworkConnectionController::wifiEventHandler(void* arg,
    esp_event_base_t base, int32_t id, void* data)
{
    auto& inst = WifiNetworkConnectionController::instance();

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "Disconnected");
        inst.setWifiConnected(false);
        NetworkManager::instance().setConnectedToWifi(false);
        esp_wifi_connect();
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        //ESP_LOGI(TAG_WIFI, "Got IP");
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        inst.setWifiConnected(true);
            //inst.wifiConnected = true
        NetworkManager::instance().setConnectedToWifi(true);

        // Crea la tarea MQTT sólo una vez
        if (!inst.taskCreated) {
            inst.taskCreated = true;
            inst.wifiConnectedCallback("---WiFi connected!---");
        }
    }
}

void WifiNetworkConnectionController::setWifiConnected(bool connected) {
    // Configura el pin LED
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << ledPin,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(ledPin, connected ? 0 : 1);
    wifiConnected = connected;

    ESP_LOGI(TAG, "###WIFI CONNECTED STATE: %s", wifiConnected ? "true" : "false");
}

bool WifiNetworkConnectionController::connectedToNetwork() {
    return wifiConnected;
}

void WifiNetworkConnectionController::mqttTask(void* pvParameters) {
    auto* inst = static_cast<WifiNetworkConnectionController*>(pvParameters);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
        inst->handleMqtt();
        //TODO esp_task_wdt_reset();
    }
}

void WifiNetworkConnectionController::handleMqtt() {
    if (!wifiConnected) {
        return;
    }
    if (!authInitDone) {
        auto authenticated = AuthenticationController::instance().init();
         ESP_LOGI(TAG, "###------------------------------------------------------");
        
        authInitDone = true;
    }
}
