#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <WiFi.h>

#include "Messages/MessageLoginRequest.h"
#include "Messages/MessageState.h"

#include "Storage.h"
#include <HTTPClient.h>

class NetworkManager{

    public:
        static NetworkManager & instance() {
            static  NetworkManager * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new NetworkManager();
            }
            return *_instance;
        }

        bool isLoggedIn(){ return loggedIn; } //TODO mutex

        void setConnectedToWifi(bool _connectedToWiFi){ connectedToWiFi = _connectedToWiFi; }
        void setConnectedToServer(bool _connectedToServer){ connectedToServer = _connectedToServer; } 

        bool connectedToWiFi = false;
        bool connectedToServer = false;
        bool loggedIn = false;

        ServerData serverData;

        void setServerAddress(String serverAddress, uint16_t serverPort, uint16_t serverMqttPort) { 
            this->serverAddress = serverAddress; 
            this->serverPort = serverPort; 
            this->serverMqttPort = serverMqttPort;
        }

        String serverAddress = "";
        uint16_t serverPort = 12345;
        uint16_t serverMqttPort = 1883;

        void setWifiCredentials(String wifiSsid, String wifiPassword){ this->wifiSsid = wifiSsid; this->wifiPassword = wifiPassword; }

        String wifiSsid = "";
        String wifiPassword = "";
        

    private:
        NetworkManager();
        String dataToSend; //TODO be careful, can overflow the whole memory with timej
        long baseMillis = 0;


        void resolveAndFollowRedirects(const char* url);
        String followRedirects(const char* url);
        void resolveIpAddress(const char* url);
        String getHostFromUrl(const String& url);
};


#endif
