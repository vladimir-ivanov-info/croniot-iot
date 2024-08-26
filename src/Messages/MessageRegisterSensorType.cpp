#include "MessageRegisterSensorType.h"

JsonObject MessageRegisterSensorType::toJson(JsonObject root) {
    JsonObject sensorObj = root["sensorType"].to<JsonObject>();

    sensor.toJson(sensorObj);

    root["deviceUuid"] = deviceUuid;
    root["deviceToken"] = deviceToken;

    return root;
}