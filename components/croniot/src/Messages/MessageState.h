#ifndef MESSAGESTATE_H
#define MESSAGESTATE_H

#include <string>

class MessageState {
public:
    MessageState(const std::string& currentTime,
                 const std::string& wifiStrength,
                 const std::string& chipTemperature,
                 const std::string& batteryVoltage);

    std::string toJson() const;

private:
    const std::string messageName = "MESSAGE_STATE";
    std::string currentTime;
    std::string wifiStrength;
    std::string chipTemperature;
    std::string batteryVoltage;
};

#endif
