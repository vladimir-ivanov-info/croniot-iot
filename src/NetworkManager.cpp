#include "NetworkManager.h"
#include <Arduino.h>

NetworkManager::NetworkManager(){

    serverData = Storage::instance().getServerData();

    serverData.serverAddress = "51.77.195.204";

    /*if(serverData.serverAddress.isEmpty()){
        resolveAndFollowRedirects("http://vladimiriot.com");
    }*/

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