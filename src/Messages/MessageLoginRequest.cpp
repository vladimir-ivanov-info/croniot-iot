#include "MessageLoginRequest.h"

MessageLoginRequest::MessageLoginRequest(String email, String password, String deviceUuid, String deviceToken) 
                                        : email(email), password(password), deviceUuid(deviceUuid), deviceToken(deviceToken) 
{
    
}

String MessageLoginRequest::toJson() {
    JsonDocument doc; // Adjust the size as needed

    doc["email"] = email;
    doc["password"] = password;
    doc["deviceUuid"] = deviceUuid;
    doc["deviceToken"] = deviceToken;

    String json;
    serializeJsonPretty(doc, json);
    return json;
}