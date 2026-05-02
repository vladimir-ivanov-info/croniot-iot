#include "MessageSensorData.h"

MessageSensorData::MessageSensorData(int sensorTypeUid, const std::string& value)
    : sensorTypeUid(sensorTypeUid), value(value) {}

std::string MessageSensorData::toString() const {
    cJSON* root = cJSON_CreateObject();

    //cJSON_AddNumberToObject(root, "sensorTypeId", sensorTypeId);
    cJSON_AddNumberToObject(root, "sensorTypeUid", sensorTypeUid);
    cJSON_AddStringToObject(root, "value", value.c_str());

    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string result(jsonStr);

    cJSON_Delete(root);
    cJSON_free(jsonStr);

    return result;
}
