#ifndef MQTTPROVIDER_H
#define MQTTPROVIDER_H

#include <memory>
#include "MqttController.h"

class MqttProvider {
    public:
        static void set(MqttController* controller) {
            instance_ = controller;
        }
    
        static MqttController* get() {
            return instance_;
        }
    
    private:
        static MqttController* instance_;
};

#endif