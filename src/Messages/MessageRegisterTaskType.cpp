#include "MessageRegisterTaskType.h"

JsonObject MessageRegisterTaskType::toJson(JsonObject root) {
    JsonObject sensorObj = root["taskType"].to<JsonObject>();

    taskType.toJson(sensorObj);

    root["deviceUuid"] = deviceUuid;
    root["deviceToken"] = deviceToken;

    return root;
}