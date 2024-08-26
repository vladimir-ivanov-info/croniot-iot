#include "MessageRegisterDevice.h"

String MessageRegisterDevice::toJson() {
    JsonDocument doc;
    doc["accountEmail"] = accountEmail;
    doc["accountPassword"] = accountPassword;
    doc["deviceUuid"] = deviceUuid;
    doc["deviceName"] = deviceName;
    doc["deviceDescription"] = deviceDescription;

    String json;
    serializeJsonPretty(doc, json);
    return json;
}