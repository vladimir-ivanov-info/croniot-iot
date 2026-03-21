#pragma once

#include <memory>
#include "esp_log.h"
#include <functional>

#include "NetworkConnectionControllerBase.h"


class NetworkConnectionControllerBase;

class NetworkConnectionProvider {
    public:

        using WifiConnectedCallback = std::function<void(const std::string& result)>;

        static bool init(NetworkConnectionControllerBase* controller, WifiConnectedCallback wifiConnectedCallback);
    
        static NetworkConnectionControllerBase* get() {
            return instance_;
        }


    private:
        static NetworkConnectionControllerBase* instance_;
};