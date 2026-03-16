/*#include "NetworkManager.h"

NetworkManager::NetworkManager(){

    serverData = Storage::instance().getServerData();

    serverData.serverAddress = "51.77.195.204";

    //if(serverData.serverAddress.isEmpty()){
    //    resolveAndFollowRedirects("http://vladimiriot.com");
    //}

}

void NetworkManager::resolveAndFollowRedirects(const char* url) {
    String finalUrl = followRedirects(url);
    Serial.print("Final URL:"); Serial.println(finalUrl);
    if (finalUrl != "") {
        resolveIpAddress(finalUrl.c_str());
    }

}

String NetworkManager::followRedirects(const char* url) {
    HTTPClient http;
    String currentUrl = url;
    bool redirect = true;

    while (redirect) {
        http.begin(currentUrl);
        int httpCode = http.GET();

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
                String location = http.getLocation();
                if (location.startsWith("/")) {
                    currentUrl = currentUrl.substring(0, currentUrl.indexOf("/", 8)) + location;
                } else {
                    currentUrl = location;
                }
                Serial.println("Redirected to: " + currentUrl);
            } else {
                redirect = false;
            }
        } else {
            Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
            return "";
        }

        http.end();
    }

    return currentUrl;
}

void NetworkManager::resolveIpAddress(const char* url) {
    String host = getHostFromUrl(url);
    if (host == "") {
        Serial.println("Invalid URL");
        return;
    }

    IPAddress ip;
    if (WiFi.hostByName(host.c_str(), ip)) {
        Serial.println("IP Address: " + ip.toString());

        ServerData serverData;
        serverData.serverAddress = ip.toString();
        Storage::instance().saveServerData(serverData);
    } else {
        Serial.println("DNS lookup failed for host: " + host);
    }
}

String NetworkManager::getHostFromUrl(const String& url) {
    int startIndex = url.indexOf("://") + 3;
    int endIndex = url.indexOf("/", startIndex);
    if (endIndex == -1) {
        endIndex = url.length();
    }
    int portIndex = url.indexOf(":", startIndex);
    if (portIndex != -1 && portIndex < endIndex) {
        endIndex = portIndex;
    }
    return url.substring(startIndex, endIndex);
}

*/

#include "NetworkManager.h"

#define TAG "NetworkManager"

#include "esp_http_client.h"   // <--- para esp_http_client_config_t, esp_http_client_init, etc.

#include <sys/socket.h>        // addrinfo, socket APIs
#include <netdb.h>             // getaddrinfo(), freeaddrinfo()
#include <arpa/inet.h>         // inet_ntop()
#include <netinet/in.h>        // struct sockaddr_in, AF_INET, INET_ADDRSTRLEN


NetworkManager::NetworkManager() {
    serverData = Storage::instance().getServerData();
    serverData.serverAddress = "51.77.195.204";
    //resolveAndFollowRedirects("http://vladimiriot.com");
}

void NetworkManager::resolveAndFollowRedirects(const std::string& url) {
    std::string finalUrl = followRedirects(url);
    ESP_LOGI(TAG, "Final URL: %s", finalUrl.c_str());
    if (!finalUrl.empty()) {
        std::string host = getHostFromUrl(finalUrl);
        resolveIpAddress(host);
    }
}

std::string NetworkManager::followRedirects(const std::string& url) {
    esp_http_client_config_t config = {
        .url = url.c_str(),
        .timeout_ms = 5000,
        .disable_auto_redirect = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    std::string finalUrl;
    if (err == ESP_OK) {
        char buffer[256];
        esp_http_client_get_url(client, buffer, sizeof(buffer));
        finalUrl = buffer;
    } else {
        ESP_LOGE(TAG, "HTTP error: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return finalUrl;
}

void NetworkManager::resolveIpAddress(const std::string& host) {
    struct addrinfo hints = {};
    struct addrinfo* res;
    hints.ai_family = AF_INET;

    int err = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (err != 0 || res == nullptr) {
        ESP_LOGE(TAG, "DNS lookup failed for host: %s", host.c_str());
        return;
    }

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipStr, sizeof(ipStr));

    ESP_LOGI(TAG, "Resolved IP: %s", ipStr);

    serverData.serverAddress = ipStr;
    Storage::instance().saveServerData(serverData);

    freeaddrinfo(res);
}

std::string NetworkManager::getHostFromUrl(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) return "";

    start += 3;
    size_t end = url.find('/', start);
    size_t portPos = url.find(':', start);
    if (portPos != std::string::npos && portPos < end) {
        end = portPos;
    }

    return url.substr(start, end - start);
}
