#include "NetworkConnectionProvider.h"

NetworkConnectionControllerBase* NetworkConnectionProvider::instance_ = nullptr;


bool NetworkConnectionProvider::init(NetworkConnectionControllerBase* controller, WifiConnectedCallback wifiConnectedCallback) {
    instance_ = controller;
        ESP_LOGI("NETWORK_PROVIDER", "NETWORK_PROVIDER Initialized!");

    return instance_->init(wifiConnectedCallback);
}