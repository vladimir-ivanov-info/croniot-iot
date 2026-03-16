#include "TaskType.h"
#include "cJSON.h"

cJSON* TaskType::toJson(cJSON* root) const {
    cJSON_AddNumberToObject(root, "uid", uid);
    cJSON_AddStringToObject(root, "name", name.c_str());
    cJSON_AddStringToObject(root, "description", description.c_str());

    cJSON* parametersJson = cJSON_AddArrayToObject(root, "parameters");

    for (const Parameter& param : parameters) {
        cJSON* paramJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(paramJson, "uid", param.uid);
        cJSON_AddStringToObject(paramJson, "name", param.name.c_str());
        cJSON_AddStringToObject(paramJson, "type", param.type.c_str());
        cJSON_AddStringToObject(paramJson, "description", param.description.c_str());
        cJSON_AddStringToObject(paramJson, "unit", param.unit.c_str());

        cJSON* constraintsJson = cJSON_AddObjectToObject(paramJson, "constraints");
        for (const auto& kv : param.constraints) {
            cJSON_AddStringToObject(constraintsJson, kv.first.c_str(), kv.second.c_str());
        }

        cJSON_AddItemToArray(parametersJson, paramJson);
    }

    return root;
}

