/*#include "MessageRegisterTaskType.h"

JsonObject MessageRegisterTaskType::toJson(JsonObject root) {
    JsonObject sensorObj = root["taskType"].to<JsonObject>();

    taskType.toJson(sensorObj);

    root["deviceUuid"] = deviceUuid;
    root["deviceToken"] = deviceToken;

    return root;
}*/

#include "MessageRegisterTaskType.h"

MessageRegisterTaskType::MessageRegisterTaskType(const std::string& deviceUuid, const std::string& deviceToken, const TaskType& taskType)
    : deviceUuid(deviceUuid), deviceToken(deviceToken), taskType(taskType) {}

/*cJSON* MessageRegisterTaskType::toJson() const {
    cJSON* root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
    cJSON_AddStringToObject(root, "deviceToken", deviceToken.c_str());

    cJSON* taskTypeJson = taskType.toJson();  // Este método debe devolver un cJSON*
    if (taskTypeJson != nullptr) {
        cJSON_AddItemToObject(root, "taskType", taskTypeJson);
    }

    return root;
}*/

cJSON* MessageRegisterTaskType::toJson(cJSON* root) const {
  // crea el sub‐objeto "taskType" y lo rellena delegando en tu TaskType
  cJSON* taskTypeObj = cJSON_AddObjectToObject(root, "taskType");
  if (taskTypeObj) {
    taskType.toJson(taskTypeObj);
  }

  // añade los campos propios
  cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
  cJSON_AddStringToObject(root, "deviceToken", deviceToken.c_str());

  return root;
}

std::string MessageRegisterTaskType::toJsonString() const {
    // construye su propio objeto raíz
    cJSON* root = cJSON_CreateObject();
    toJson(root);

    // serializa
    char* buf = cJSON_PrintUnformatted(root);
    std::string s = buf ? buf : "";

    // libera todo
    if (buf) cJSON_free(buf);
    cJSON_Delete(root);

    return s;
}