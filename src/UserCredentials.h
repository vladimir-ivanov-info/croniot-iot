#ifndef USERCREDENTIALS_H
#define USERCREDENTIALS_H

#include "Arduino.h"

class UserCredentials{

    public:
        UserCredentials(){}

        UserCredentials(String accountEmail, String accountUuid, String accountPassword, String deviceUuid, String deviceToken, String deviceName, String deviceDescription) 
        
        : accountEmail(accountEmail), accountUuid(accountUuid), accountPassword(accountPassword), deviceUuid(deviceUuid), deviceToken(deviceToken), deviceName(deviceName), deviceDescription(deviceDescription)
        
        {

        }
        
        String accountEmail;
        String accountUuid;
        String accountPassword;
        String deviceUuid;
        String deviceToken;
        String deviceName;
        String deviceDescription;

};

#endif