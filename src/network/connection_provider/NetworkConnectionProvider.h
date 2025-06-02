#ifndef NETWORKCONNECTIONPROVIDER_H
#define NETWORKCONNECTIONPROVIDER_H

#include <memory>

#include "NetworkConnectionControllerBase.h"

class NetworkConnectionProvider {
    public:
        static bool init(NetworkConnectionControllerBase* controller) {
            instance_ = controller;
            return instance_->init();
        }
    
        static NetworkConnectionControllerBase* get() {
            return instance_;
        }


    private:
        static NetworkConnectionControllerBase* instance_;
};

#endif

