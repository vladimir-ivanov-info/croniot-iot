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
        Result publish(String topic, String message) override;
        void registerCallback(String topic, TaskBase* taskInstance) override;


private:


};

#endif