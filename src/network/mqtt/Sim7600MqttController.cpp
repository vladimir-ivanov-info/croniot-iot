#include "Sim7600MqttController.h"

bool Sim7600MqttController::init(){
    //TODO
    return true;
}

Result Sim7600MqttController::publish(String topic, String message){
    return Sim7600::instance().mqttPublish(topic, message);
}

void Sim7600MqttController::registerCallback(String topic, TaskBase* taskInstance){
    //TODO
}