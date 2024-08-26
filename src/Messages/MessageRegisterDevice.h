#ifndef MESSAGEREGISTERDEVICE_H
#define MESSAGEREGISTERDEVICE_H

#include <ArduinoJson.h>

class MessageRegisterDevice {
public:

    MessageRegisterDevice(String accountEmail, String accountPassword, String deviceUuid, String deviceName, String deviceDescription) : accountEmail(accountEmail), accountPassword(accountPassword), deviceUuid(deviceUuid), deviceName(deviceName), deviceDescription(deviceDescription) {}

    String toJson();

private:
    String accountEmail;
    String accountPassword;
    String deviceUuid;
    String deviceName;
    String deviceDescription;
};

#endif
