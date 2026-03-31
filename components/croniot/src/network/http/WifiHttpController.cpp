#include "WifiHttpController.h"
#include "../NetworkManager.h"
#include "esp_log.h"
#include "esp_http_client.h"

#define TAG "WifiHttpController"

#include "WifiHttpController.h"
#include "../NetworkManager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include <cstring>

#define TAG "WifiHttpController"

static Result make_error(const char *msg) { return {false, msg}; }

Result WifiHttpController::sendHttpPost(const std::string& payload,
                                        const std::string& route)
{
    Result result(false, "Default result");

    /* ---------- Construir URL ---------- */
    const std::string host  = NetworkManager::instance().serverAddress;
    const uint16_t    port  = NetworkManager::instance().serverPort;
    const std::string url   = "http://" + host + ":" + std::to_string(port) + route;

    ESP_LOGI(TAG, "POST %s : %s", url.c_str(), payload.c_str());

    /* ---------- Configuración ---------- */
    esp_http_client_config_t cfg{};
    cfg.url              = url.c_str();          // se envía la URL completa
    cfg.host             = host.c_str();         // solo informativo
    cfg.port             = port;
    cfg.method           = HTTP_METHOD_POST;
    cfg.timeout_ms       = 10'000;               // 10 s
    cfg.transport_type   = HTTP_TRANSPORT_OVER_TCP;
    cfg.buffer_size      = 1024;                 // RX
    cfg.buffer_size_tx   = 1024;                 // TX
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count = 3;

    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) return Result(false, "http init failed");

    /* ---------- Cabeceras ---------- */
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_header(cli, "Accept",       "application/json");

    /* ---------- Intentar con backoff exponencial ---------- */
    static constexpr int MAX_RETRIES = 3;
    static constexpr int BASE_DELAY_MS = 2000;  // 2s, 4s, 8s

    for (int attempt = 0; attempt <= MAX_RETRIES; ++attempt) {
        if (attempt > 0) {
            int delay_ms = BASE_DELAY_MS * (1 << (attempt - 1));
            ESP_LOGW(TAG, "Retry %d/%d in %d ms...", attempt, MAX_RETRIES, delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        /* ---------- Abrir conexión ---------- */
        esp_err_t err = esp_http_client_open(cli, payload.length());
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Connection failed: %s", esp_err_to_name(err));
            if (attempt == MAX_RETRIES) {
                esp_http_client_cleanup(cli);
                return Result(false, esp_err_to_name(err));
            }
            continue;
        }

        /* ---------- Enviar cuerpo ---------- */
        int wr = esp_http_client_write(cli, payload.c_str(), payload.length());
        if (wr < 0 || static_cast<size_t>(wr) != payload.length()) {
            ESP_LOGW(TAG, "Write failed");
            esp_http_client_close(cli);
            if (attempt == MAX_RETRIES) {
                esp_http_client_cleanup(cli);
                return Result(false, "write failed");
            }
            continue;
        }

        /* ---------- Recibir respuesta ---------- */
        int status      = esp_http_client_fetch_headers(cli);
        int content_len = esp_http_client_get_content_length(cli);

        ESP_LOGI(TAG, "HTTP %d, Content-Length hdr=%d", status, content_len);

        std::string body;
        if (content_len > 0) body.reserve(content_len);

        char buf[256];
        int  rlen;
        while ((rlen = esp_http_client_read(cli, buf, sizeof(buf))) > 0) {
            body.append(buf, rlen);
        }
        ESP_LOGI(TAG, "Body (%zu B): %s", body.size(), body.c_str());

        /* ---------- Interpretar ---------- */
        if (!body.empty()) {
            result = parseResult(body);
        } else {
            result = Result(false, "empty body");
        }

        /* ---------- Liberar ---------- */
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return result;
    }

    /* No debería llegar aquí, pero por seguridad */
    esp_http_client_cleanup(cli);
    return Result(false, "max retries exceeded");
}