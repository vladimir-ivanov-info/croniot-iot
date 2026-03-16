#pragma once
#include "ConnectionTypes.h"

#include "esp_task_wdt.h"


#include <string>

#include "NetworkConnectionProvider.h"


class NetworkConnectionControllerBase {
public:


    virtual bool init(connection::WifiConnectedCallback wifiConnectedCallback) = 0;
    virtual bool connectedToNetwork() = 0;  // Puedes dejarlo como método virtual no puro si tiene implementación

    virtual ~NetworkConnectionControllerBase() = default;

protected:
    connection::WifiConnectedCallback wifiConnectedCallback {};

private:

    

};
