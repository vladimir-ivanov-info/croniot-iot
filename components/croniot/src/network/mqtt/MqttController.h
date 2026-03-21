#ifndef MQTTCONTROLLER_H
#define MQTTCONTROLLER_H

#include <string>
#include "../../Result.h"
#include "../../Tasks/TaskBase.h"

class MqttController {
public:
    bool initialized = false;

    virtual bool init() = 0;

    virtual Result publish(const std::string& topic, const std::string& message) = 0;

    virtual void registerCallback(const std::string& topic, TaskBase* taskInstance) = 0;

    virtual void registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance) = 0;

    virtual ~MqttController() = default;
};

#endif
