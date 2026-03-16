#ifndef SIM7600MQTTCONTROLLER_H
#define SIM7600MQTTCONTROLLER_H

#include "MqttController.h"

#include "network/sim7600/Sim7600.h"


class Sim7600MqttController : public MqttController {

    public:

        static Sim7600MqttController& instance() {
            static Sim7600MqttController _instance;  // No need to use a pointer
            return _instance;
        }

        bool init() override;
        Result publish(const std::string& topic, const std::string& message) override;
        void registerCallback(const std::string& topic, TaskBase* taskInstance) override;

        void registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance) override;


private:


};

#endif