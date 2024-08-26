#ifndef MESSAGELOGIN_H
#define MESSAGELOGIN_H

#include <ArduinoJson.h>

class MessageLogin {
public:
    MessageLogin(String accountEmail, String accountPassword, String deviceUuid, String deviceToken); // Constructor
    String toJson();

private:
    String accountEmail;
    String accountPassword;
    String deviceUuid;
    String deviceToken;
};

#endif
