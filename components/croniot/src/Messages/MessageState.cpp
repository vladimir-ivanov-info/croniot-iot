#include "MessageState.h"
#include "cJSON.h"

MessageState::MessageState(const std::string& currentTime,
                           const std::string& wifiStrength,
                           const std::string& chipTemperature,
                           const std::string& batteryVoltage)
    : currentTime(currentTime),
      wifiStrength(wifiStrength),
      chipTemperature(chipTemperature),
      batteryVoltage(batteryVoltage) {}

std::string MessageState::toJson() const {
    cJSON* root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "currentTime", currentTime.c_str());
    cJSON_AddStringToObject(root, "wifiStrength", wifiStrength.c_str());
    cJSON_AddStringToObject(root, "chipTemperature", chipTemperature.c_str());
    cJSON_AddStringToObject(root, "batteryVoltage", batteryVoltage.c_str());

    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string result(jsonStr);

    cJSON_Delete(root);
    free(jsonStr);

    return result;
}
