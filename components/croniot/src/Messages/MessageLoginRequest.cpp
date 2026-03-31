#include "MessageLoginRequest.h"
#include "cJSON.h"

MessageLoginRequest::MessageLoginRequest(const std::string& email, const std::string& password,
                                         const std::string& deviceUuid, const std::string& deviceToken)
    : email(email), password(password), deviceUuid(deviceUuid), deviceToken(deviceToken) {}

std::string MessageLoginRequest::toJson() const {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "email", email.c_str());
    cJSON_AddStringToObject(root, "password", password.c_str());
    cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
    cJSON_AddStringToObject(root, "deviceToken", deviceToken.c_str());

    char *jsonStr = cJSON_Print(root);

    std::string json = jsonStr;

    cJSON_Delete(root);
    free(jsonStr);

    return json;
}
