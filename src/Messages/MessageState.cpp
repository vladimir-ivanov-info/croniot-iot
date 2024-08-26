#include "MessageState.h"

MessageState::MessageState(const String& currentTime, const String& wifiStrength, const String& chipTemperature,  const String& batteryVoltage) {
    this->currentTime = currentTime;
    this->wifiStrength = wifiStrength;
    this->chipTemperature = chipTemperature;
    this->batteryVoltage = batteryVoltage;
}

String MessageState::toJson() {
    JsonDocument doc;
    doc["currentTime"] = currentTime;
    doc["wifiStrength"] = wifiStrength;
    doc["chipTemperature"] = chipTemperature;
    doc["batteryVoltage"] = batteryVoltage;

    String json;
    serializeJsonPretty(doc, json);
    return json;
}