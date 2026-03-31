#include "MessageRegisterTaskType.h"

MessageRegisterTaskType::MessageRegisterTaskType(const std::string& deviceUuid, const std::string& deviceToken, const TaskType& taskType)
    : deviceUuid(deviceUuid), deviceToken(deviceToken), taskType(taskType) {}


cJSON* MessageRegisterTaskType::toJson(cJSON* root) const {
  cJSON* taskTypeObj = cJSON_AddObjectToObject(root, "taskType");
  if (taskTypeObj) {
    taskType.toJson(taskTypeObj);
  }

  cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
  cJSON_AddStringToObject(root, "deviceToken", deviceToken.c_str());

  return root;
}

std::string MessageRegisterTaskType::toJsonString() const {
    cJSON* root = cJSON_CreateObject();
    toJson(root);

    char* buf = cJSON_PrintUnformatted(root);
    std::string s = buf ? buf : "";

    if (buf) cJSON_free(buf);
    cJSON_Delete(root);

    return s;
}