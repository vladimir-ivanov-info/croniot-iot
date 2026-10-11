#include "Sim7600MqttController.h"

bool Sim7600MqttController::init() {
    // TODO: Aquí puedes inicializar MQTT si tu módulo SIM7600 lo requiere (ej. AT+CMQTTSTART)
    return true;
}

Result Sim7600MqttController::publish(const std::string& topic, const std::string& message) {
    return Sim7600::instance().mqttPublish(topic, message);
}

Result Sim7600MqttController::publishWithOptions(const std::string& topic, const std::string& message,
                                                  int /*qos*/, bool /*retain*/) {
    // TODO (Tanda D follow-up): AT+CMQTTWILL exists for LWT and the AT
    // command set supports a QoS/retain flag per publish, but wiring
    // that through Sim7600 hasn't happened yet - same "not yet
    // implemented, not silently wrong" gap as registerCallback() below.
    // Falls back to the existing fire-and-forget publish() so a caller
    // still gets *a* delivery attempt rather than nothing.
    return Sim7600::instance().mqttPublish(topic, message);
}

void Sim7600MqttController::registerCallback(const std::string& topic, TaskBase* taskInstance) {
    // TODO: Guardar en un mapa `topic -> taskInstance`
    // Luego, cuando recibas un mensaje MQTT, busca el task correspondiente y llama a su método.
}

void Sim7600MqttController::registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance){

}

void Sim7600MqttController::registerRawCallback(const std::string& /*topic*/,
                                                 std::function<void(const std::string&)> /*callback*/) {
    // TODO (Tanda D follow-up): the uplink's ack/log_config subscriptions
    // need this to actually control the sync cursor over a cellular
    // link - deferred along with registerCallback() above rather than
    // guessed at without SIM7600 hardware to verify against.
}