#include "Result.h"
#include "cJSON.h"
//#include "cjson/cJSON.h"
//#include <cjson/cJSON.h>


std::string Result::toString() const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "message", message.c_str());

    char* json = cJSON_Print(root);  // Use cJSON_PrintUnformatted for compact
    std::string resultStr = json ? json : "";

    if (json) {
        cJSON_free(json);
    }
    cJSON_Delete(root);

    return resultStr;
}
