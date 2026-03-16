#include "driver/gpio.h"
#include "Sim7600.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "SIM7600";

std::string SIM_PIN_EXTERN;  // define en tu main o configuración

Sim7600& Sim7600::instance() {
    static Sim7600 inst;
    return inst;
}

Sim7600::Sim7600() {
    // inicializa GPIO de POWER
    gpio_set_direction((gpio_num_t)PIN_PWRK, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_PWRK, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level((gpio_num_t)PIN_PWRK, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    uartInit();
}

void Sim7600::uartInit() {
    uart_config_t cfg = {
        .baud_rate = SIM7600_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(SIM7600_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SIM7600_UART_NUM,
                                SIM7600_UART_TX_PIN,
                                SIM7600_UART_RX_PIN,
                                UART_PIN_NO_CHANGE,
                                UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(SIM7600_UART_NUM,
                                       SIM7600_UART_BUF_SIZE * 2,
                                       0, 0, nullptr, 0));
}

void Sim7600::uartWrite(const std::string& data) {
    uart_write_bytes(SIM7600_UART_NUM, data.c_str(), data.size());
    uart_write_bytes(SIM7600_UART_NUM, "\r\n", 2);
}

size_t Sim7600::uartRead(char* buf, size_t len, uint32_t timeoutMs) {
    return uart_read_bytes(SIM7600_UART_NUM, (uint8_t*)buf, len, pdMS_TO_TICKS(timeoutMs));
}

bool Sim7600::init() {
    ESP_LOGI(TAG, "🔌 Inicializando SIM7600...");
    initialized = false;

    // prueba AT sin reset
    uartWrite("AT");
    if (getResponse(2000).find("OK") == std::string::npos) {
        ESP_LOGW(TAG, "No responde, reiniciando módulo...");
        uartWrite("AT+CRESET");
        vTaskDelay(pdMS_TO_TICKS(2000));
        uartWrite("AT");
        if (getResponse(2000).find("OK") == std::string::npos) {
            ESP_LOGE(TAG, "❌ No se recuperó SIM7600 tras CRESET.");
            return false;
        }
    }

    // comprueba PIN
    uartWrite("AT+CPIN?");
    auto resp = getResponse();
    AT_CPIN_RESPONSE cpin = parseCpin(resp);
    if (cpin == SIM_PIN) {
        uartWrite("AT+CPIN=\"" + SIM_PIN_EXTERN + "\"");
        resp = getResponse(2000);
        if (resp.find("OK") == std::string::npos) {
            ESP_LOGE(TAG, "❌ PIN incorrecto.");
            return false;
        }
    } else if (cpin != OK && cpin != RDY) {
        ESP_LOGE(TAG, "❌ Estado SIM desconocido: %s", resp.c_str());
        return false;
    }

    // configurar APN
    uartWrite("AT+CGDCONT=1,\"IP\",\"orangeworld\"");
    if (getResponse(2000).find("OK") == std::string::npos) {
        ESP_LOGE(TAG, "❌ Error en APN");
        return false;
    }

    // abrir red
    uartWrite("AT+NETOPEN");
    if (getResponse(5000).find("OK") == std::string::npos) {
        ESP_LOGE(TAG, "❌ NETOPEN falló");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "✅ SIM7600 inicializado.");
    return true;
}

std::string Sim7600::getResponse(uint32_t timeoutMs) {
    std::string out;
    char buf[256];
    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeoutMs)) {
        size_t len = uartRead(buf, sizeof(buf)-1, 100);
        if (len) {
            buf[len] = '\0';
            out += buf;
            // si ya viene OK o ERROR, rompemos
            if (out.find("OK") != std::string::npos ||
                out.find("ERROR") != std::string::npos) {
                break;
            }
        }
    }
    return out;
}

Result Sim7600::sendHttpPost(const std::string& content, const std::string& route) {
    if (!initialized && !init()) {
        return Result(false, "No se pudo inicializar SIM7600");
    }
    ESP_LOGI(TAG, "📡 HTTP POST: %s", route.c_str());

    uartWrite("AT+HTTPINIT");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "HTTPINIT failed");
    }

    uartWrite("AT+HTTPPARA=\"URL\",\"" + std::string("http://") + route + "\"");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "Error configurando URL");
    }

    uartWrite("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "Error configurando CONTENT");
    }

    uartWrite("AT+HTTPDATA=" + std::to_string(content.size()) + ",10000");
    if (getResponse().find("DOWNLOAD") == std::string::npos) {
        return Result(false, "HTTPDATA no iniciado");
    }

    uartWrite(content);
    vTaskDelay(pdMS_TO_TICKS(1500));

    uartWrite("AT+HTTPACTION=1");
    vTaskDelay(pdMS_TO_TICKS(8000));
    auto actionResp = getResponse();
    ESP_LOGI(TAG, "HTTPACTION: %s", actionResp.c_str());

    // parse +HTTPACTION: 1,200,<len>
    size_t pos = actionResp.find(",200,");
    if (pos == std::string::npos) {
        return Result(false, "HTTPACTION != 200");
    }
    int length = std::stoi(actionResp.substr(pos+5));
    uartWrite("AT+HTTPREAD=0," + std::to_string(length));
    auto readResp = getResponse(5000);
    ESP_LOGI(TAG, "HTTPREAD: %s", readResp.c_str());

    uartWrite("AT+HTTPTERM");
    return parseResult(readResp);
}

Result Sim7600::mqttPublish(const std::string& topic, const std::string& message) {
    if (!initialized && !init()) {
        return Result(false, "SIM7600 init failed");
    }
    ESP_LOGI(TAG, "📡 MQTT publish: %s", topic.c_str());

    uartWrite("AT+CMQTTSTART");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "CMQTTSTART falló");
    }

    uartWrite("AT+CMQTTACCQ=0,\"SIM7600_Client\"");
    getResponse();

    uartWrite("AT+CMQTTCONNECT=0,\"tcp://51.77.195.204:1883\",60,1");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "CMQTTCONNECT falló");
    }

    // esperar URC…
    vTaskDelay(pdMS_TO_TICKS(500));
    uartWrite("AT+CMQTTTOPIC=0," + std::to_string(topic.size()));
    getResponse();
    uartWrite(topic);
    getResponse();

    uartWrite("AT+CMQTTPAYLOAD=0," + std::to_string(message.size()));
    getResponse();
    uartWrite(message);
    getResponse();

    uartWrite("AT+CMQTTPUB=0,1,60");
    if (getResponse().find("OK") == std::string::npos) {
        return Result(false, "CMQTTPUB falló");
    }
    return Result(true, "Publicado con éxito");
}

Sim7600::AT_CPIN_RESPONSE Sim7600::parseCpin(const std::string& resp) {
    if (resp.find("SIM PIN") != std::string::npos) return SIM_PIN;
    if (resp.find("READY")  != std::string::npos) return RDY;
    if (resp.find("OK")     != std::string::npos) return OK;
    return UNDEFINED;
}

std::string Sim7600::extractJson(const std::string& input) {
    auto a = input.find('{'), b = input.rfind('}');
    if (a!=std::string::npos && b!=std::string::npos && b>a)
        return input.substr(a, b-a+1);
    return "";
}

Result Sim7600::parseResult(const std::string& raw) {
    auto j = extractJson(raw);
    // parse con cJSON o ArduinoJson migrado…
    // aquí asumo una función auxiliar que devuelve Result
    // …
    return Result(true, j);
}
