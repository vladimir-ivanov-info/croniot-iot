#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <string>
#include <cstdint>
#include "../Messages/MessageLoginRequest.h"
#include "../Messages/MessageState.h"
#include "../Storage.h"

class NetworkManager {

public:
    static NetworkManager& instance() {
        static NetworkManager _instance;
        return _instance;
    }

    bool isLoggedIn() const { return loggedIn; }

    void setConnectedToWifi(bool connected) { connectedToWiFi = connected; }
    void setConnectedToServer(bool connected) { connectedToServer = connected; }

    bool connectedToWiFi = false;
    bool connectedToServer = false;
    bool loggedIn = false;

    ServerData serverData;

    void setServerAddress(const std::string& address, uint16_t port, uint16_t mqttPort) {
        serverAddress = address;
        serverPort = port;
        serverMqttPort = mqttPort;
    }

    std::string serverAddress;
    uint16_t serverPort = 12345;
    uint16_t serverMqttPort = 1883;

    void setWifiCredentials(const std::string& ssid, const std::string& password) {
        wifiSsid = ssid;
        wifiPassword = password;
    }

    std::string wifiSsid;
    std::string wifiPassword;

private:
    NetworkManager();

    std::string dataToSend;
    long baseMillis = 0;

    void resolveAndFollowRedirects(const std::string& url);
    std::string followRedirects(const std::string& url);
    void resolveIpAddress(const std::string& url);
    std::string getHostFromUrl(const std::string& url);
};

#endif
