#ifndef NETWORKCONNECTIONCONTROLLERBASE_H
#define NETWORKCONNECTIONCONTROLLERBASE_H

#include "esp_task_wdt.h"

class NetworkConnectionControllerBase {
    public:
        virtual bool init() = 0;

        virtual bool connectedToNetwork();


        virtual ~NetworkConnectionControllerBase() = default;
};

#endif
    