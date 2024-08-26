#ifndef MESSAGESTATE_H
#define MESSAGESTATE_H

#include <ArduinoJson.h>

class MessageState {
public:
    MessageState(const String& currentTime, const String& wifiStrength, const String& chipTemperature, const String& batteryVoltage); // Constructor

    String toJson();

private:
    String messageName = "MESSAGE_STATE";
    String currentTime;
    String wifiStrength;
    String chipTemperature;
    String batteryVoltage;
};

#endif
