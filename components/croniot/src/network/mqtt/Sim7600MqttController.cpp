#include "Sim7600MqttController.h"

bool Sim7600MqttController::init() {
    // TODO: Aquí puedes inicializar MQTT si tu módulo SIM7600 lo requiere (ej. AT+CMQTTSTART)
    return true;
}

Result Sim7600MqttController::publish(const std::string& topic, const std::string& message) {
    return Sim7600::instance().mqttPublish(topic, message);
}

void Sim7600MqttController::registerCallback(const std::string& topic, TaskBase* taskInstance) {
    // TODO: Guardar en un mapa `topic -> taskInstance`
    // Luego, cuando recibas un mensaje MQTT, busca el task correspondiente y llama a su método.
}

void Sim7600MqttController::registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance){

}