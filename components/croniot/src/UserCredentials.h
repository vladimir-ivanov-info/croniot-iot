#ifndef USERCREDENTIALS_H
#define USERCREDENTIALS_H

#include <string>

class UserCredentials {
public:
    UserCredentials() = default;

    UserCredentials(const std::string& accountEmail,
                    const std::string& accountUuid,
                    const std::string& accountPassword,
                    const std::string& deviceUuid,
                    const std::string& deviceToken,
                    const std::string& deviceName,
                    const std::string& deviceDescription)
        : accountEmail(accountEmail),
          accountUuid(accountUuid),
          accountPassword(accountPassword),
          deviceUuid(deviceUuid),
          deviceToken(deviceToken),
          deviceName(deviceName),
          deviceDescription(deviceDescription)
    {}

    std::string accountEmail;
    std::string accountUuid;
    std::string accountPassword;
    std::string deviceUuid;
    std::string deviceToken;
    std::string deviceName;
    std::string deviceDescription;
};

#endif
