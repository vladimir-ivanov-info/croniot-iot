#include "MessageRegisterSensorType.h"
#include "cJSON.h"

MessageRegisterSensorType::MessageRegisterSensorType(const std::string& deviceUuid,
                                                     const std::string& deviceToken,
                                                     const SensorType& sensor)
    : deviceUuid(deviceUuid), deviceToken(deviceToken), sensor(sensor) {}

cJSON* MessageRegisterSensorType::toJson() const {
    cJSON* root = cJSON_CreateObject();

    // Agregar UUID y token
    cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
    cJSON_AddStringToObject(root, "deviceToken", deviceToken.c_str());

    // Agregar sensorType como objeto anidado
    cJSON* sensorObj = cJSON_CreateObject();
    sensor.toJson(sensorObj);  // Este método debe rellenar el objeto cJSON con los campos del sensor
    cJSON_AddItemToObject(root, "sensorType", sensorObj);

    return root;
}

std::string MessageRegisterSensorType::toJsonString() const {
    cJSON* root = toJson();
    char* buf = cJSON_PrintUnformatted(root);
    std::string s = buf ? buf : "";
    if (buf) cJSON_free(buf);
    cJSON_Delete(root);
    return s;
}