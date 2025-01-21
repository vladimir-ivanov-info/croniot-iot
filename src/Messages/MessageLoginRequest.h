#ifndef MESSAGELOGINREQUEST_H
#define MESSAGELOGINREQUEST_H

#include <ArduinoJson.h>

class MessageLoginRequest {
    
public:
    MessageLoginRequest(String email, String password, String deviceUuid, String deviceToken);
    String toJson();

private:
    String email;
    String password;
    String deviceUuid;
    String deviceToken;
};

#endif
