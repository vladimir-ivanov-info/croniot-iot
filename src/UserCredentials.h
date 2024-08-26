#ifndef USERCREDENTIALS_H
#define USERCREDENTIALS_H

#include "Arduino.h"

class UserCredentials{

    public:
        UserCredentials(){}

        UserCredentials(String accountEmail, String accountPassword, String deviceUuid, String deviceToken) : accountEmail(accountEmail), accountPassword(accountPassword), deviceUuid(deviceUuid), deviceToken(deviceToken){

        }
        
        String accountEmail;
        String accountPassword;
        String deviceUuid;
        String deviceToken;

};

#endif