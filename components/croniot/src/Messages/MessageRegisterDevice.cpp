#include "MessageRegisterDevice.h"
#include "cJSON.h"

MessageRegisterDevice::MessageRegisterDevice(const std::string& accountEmail,
                                             const std::string& accountPassword,
                                             const std::string& deviceUuid,
                                             const std::string& deviceName,
                                             const std::string& deviceDescription)
    : accountEmail(accountEmail), accountPassword(accountPassword),
      deviceUuid(deviceUuid), deviceName(deviceName), deviceDescription(deviceDescription) {}

    std::string MessageRegisterDevice::toJson() const {
        cJSON *root = cJSON_CreateObject();

        cJSON_AddStringToObject(root, "accountEmail", accountEmail.c_str());
        cJSON_AddStringToObject(root, "accountPassword", accountPassword.c_str());
        cJSON_AddStringToObject(root, "deviceUuid", deviceUuid.c_str());
        cJSON_AddStringToObject(root, "deviceName", deviceName.c_str());
        cJSON_AddStringToObject(root, "deviceDescription", deviceDescription.c_str());

        char *jsonStr = cJSON_PrintUnformatted(root);
        std::string result(jsonStr);

        cJSON_Delete(root);
        free(jsonStr);

    return result;
}
