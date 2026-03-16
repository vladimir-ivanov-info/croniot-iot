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

// Result WifiHttpController::sendHttpPost(const std::string& content, const std::string& route) {
//     Result result(false, "Default result");

//     std::string serverAddress = NetworkManager::instance().serverAddress;
//     uint16_t port = NetworkManager::instance().serverPort;

//     std::string fullUrl = "http://" + serverAddress + ":" + std::to_string(port) + route;

//     ESP_LOGI(TAG, "Sending POST to URL: %s : %s", fullUrl.c_str(), content.c_str());

//     // Debug IP y gateway actuales
//     esp_netif_ip_info_t ip_info;
//     if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info) == ESP_OK) {
//         ESP_LOGI(TAG, "Device IP: " IPSTR, IP2STR(&ip_info.ip));
//         ESP_LOGI(TAG, "Gateway:   " IPSTR, IP2STR(&ip_info.gw));
//     } else {
//         ESP_LOGW(TAG, "Failed to get IP info");
//     }

//     // Copiar URL a buffer seguro para mantener la referencia
//     char urlBuffer[256];
//     strncpy(urlBuffer, fullUrl.c_str(), sizeof(urlBuffer));
//     urlBuffer[sizeof(urlBuffer) - 1] = '\0';

//     // Configuración del cliente HTTP
//     esp_http_client_config_t config = {};
//     config.url = urlBuffer;
//     config.host = serverAddress.c_str(); // Redundante si usas IP directa, pero útil
//     config.port = port;
//     config.method = HTTP_METHOD_POST;
//     config.timeout_ms = 10000;
//     config.transport_type = HTTP_TRANSPORT_OVER_TCP;
//     config.buffer_size = 1024;
//     config.buffer_size_tx = 1024;
//     config.keep_alive_enable = false;  // Prueba explícitamente sin keep-alive


//     esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);


//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     if (!client) {
//         ESP_LOGE(TAG, "Failed to initialize HTTP client");
//         return Result(false, "Client init failed");
//     }

//     esp_http_client_set_header(client, "Content-Type", "application/json");
//     esp_http_client_set_post_field(client, content.c_str(), content.length());


//     esp_log_level_set("esp_http_client", ESP_LOG_VERBOSE);
//     esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);
//     esp_log_level_set("transport_tls", ESP_LOG_VERBOSE);  // incluso si usas TCP


//     esp_http_client_set_header(client, "Accept-Encoding", "identity");

//     // Perform request
//     esp_err_t err = esp_http_client_perform(client);
//     if (err == ESP_OK) {
//         // … después de esp_http_client_perform(client);
//         int status   = esp_http_client_get_status_code(client);
//         int64_t size = esp_http_client_get_content_length(client);  // puede ser -1
//         ESP_LOGI(TAG, "HTTP %d, body len (Content-Length) = %lld", status, size);

//         /* === leer el cuerpo COMPLETO === */
//         std::string response;
// size_t expected = (size_t) esp_http_client_get_content_length(client);
// std::string body;
// body.reserve(expected > 0 ? expected : 256);

// char buf[256];
// while (body.size() < expected || expected == 0 /*desconocido*/) {
//     int n = esp_http_client_read(client, buf, sizeof(buf));
//     if (n < 0) {                      // error
//         ESP_LOGE(TAG, "read error");
//         break;
//     }
//     if (n == 0) {                     // nada disponible todavía
//         vTaskDelay(pdMS_TO_TICKS(50));  // cede la CPU y vuelve a probar
//         continue;
//     }
//     body.append(buf, n);
// }

//         ESP_LOGI(TAG, "Full body (%zu bytes): %s", response.size(), response.c_str());

//         if (!response.empty())
//             result = parseResult(response);
//         else
//             result = Result(false, "Empty body");
//     } else {
//         ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
//         result = Result(false, esp_err_to_name(err));
//     }



//     esp_http_client_cleanup(client);
//     return result;
// }




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

    /* ---------- Abrir conexión ---------- */
    esp_err_t err = esp_http_client_open(cli, payload.length());
    if (err != ESP_OK) {
        esp_http_client_cleanup(cli);
        return Result(false, esp_err_to_name(err));
    }

    /* ---------- Enviar cuerpo ---------- */
    int wr = esp_http_client_write(cli, payload.c_str(), payload.length());
    if (wr < 0 || static_cast<size_t>(wr) != payload.length()) {
        esp_http_client_close(cli);               // cerrar primero
        esp_http_client_cleanup(cli);
        return Result(false, "write failed");
    }

    /* ---------- Recibir respuesta ---------- */
    int status      = esp_http_client_fetch_headers(cli);   // bloquea hasta cabeceras
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
        result = parseResult(body);               // tu función de parseo
    } else {
        result = Result(false, "empty body");
    }

    /* ---------- Liberar ---------- */
    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    return result;
}