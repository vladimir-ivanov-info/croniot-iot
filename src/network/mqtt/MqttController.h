#ifndef MQTTCONTROLLER_H
#define MQTTCONTROLLER_H

#include <Arduino.h>
#include "Result.h"

#include "../../Tasks/TaskBase.h"

class MqttController {

    public:

        volatile bool initialized = false;

        virtual bool init() = 0;
        virtual Result publish(String topic, String message) = 0;
        virtual void registerCallback(String topic, TaskBase* taskInstance) = 0;

        virtual ~MqttController() = default;
};
    

#endif