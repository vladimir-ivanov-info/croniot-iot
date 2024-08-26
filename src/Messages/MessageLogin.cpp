#include "MessageLogin.h"

MessageLogin::MessageLogin(String accountEmail, String accountPassword, String deviceUuid, String deviceToken) : accountEmail(accountEmail), accountPassword(accountPassword), deviceUuid(deviceUuid), deviceToken(deviceToken) {

}

String MessageLogin::toJson() {
    JsonDocument doc; // Adjust the size as needed

    doc["accountEmail"] = accountEmail;
    doc["accountPassword"] = accountPassword;
    doc["deviceUuid"] = deviceUuid;
    doc["deviceToken"] = deviceToken;

    String json;
    serializeJsonPretty(doc, json);
    return json;
}