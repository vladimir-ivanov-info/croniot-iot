#pragma once

//#ifndef NETWORKCONNECTIONPROVIDER_H
//#define NETWORKCONNECTIONPROVIDER_H

#include <memory>
#include "esp_log.h"
#include <functional>

#include "NetworkConnectionControllerBase.h"


class NetworkConnectionControllerBase;

class NetworkConnectionProvider {
    public:

        using WifiConnectedCallback = std::function<void(const std::string& result)>;


       /* static bool init(NetworkConnectionControllerBase* controller, WifiConnectedCallback wifiConnectedCallback) {
            instance_ = controller;
                ESP_LOGI("NETWORK_PROVIDER", "NETWORK_PROVIDER Initialized!");

            return instance_->init(wifiConnectedCallback);
        }*/


        static bool init(NetworkConnectionControllerBase* controller, WifiConnectedCallback wifiConnectedCallback);
    
        static NetworkConnectionControllerBase* get() {
            return instance_;
        }


    private:
        static NetworkConnectionControllerBase* instance_;
};

//#endif

