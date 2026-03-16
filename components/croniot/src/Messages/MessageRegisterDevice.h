#ifndef MESSAGEREGISTERDEVICE_H
#define MESSAGEREGISTERDEVICE_H

#include <string>

class MessageRegisterDevice {

public:
    MessageRegisterDevice(const std::string& accountEmail,
                          const std::string& accountPassword,
                          const std::string& deviceUuid,
                          const std::string& deviceName,
                          const std::string& deviceDescription);

    std::string toJson() const;

private:
    std::string accountEmail;
    std::string accountPassword;
    std::string deviceUuid;
    std::string deviceName;
    std::string deviceDescription;
};

#endif
