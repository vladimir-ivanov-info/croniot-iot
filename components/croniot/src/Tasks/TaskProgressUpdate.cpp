#include <string>  // Asegura que std::string está bien definido
#include "cJSON.h"
#include "TaskProgressUpdate.h"

std::string TaskProgressUpdate::toJson() const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "taskTypeUid", taskTypeUid);
    cJSON_AddNumberToObject(root, "taskUid", taskUid);
    cJSON_AddStringToObject(root, "state", state.c_str());
    cJSON_AddNumberToObject(root, "progress", progress);
    cJSON_AddStringToObject(root, "errorMessage", errorMessage.c_str());

    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string result = jsonStr ? jsonStr : "";

    cJSON_free(jsonStr);
    cJSON_Delete(root);

    return result;
}
