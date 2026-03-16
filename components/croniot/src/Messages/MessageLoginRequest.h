/*#ifndef MESSAGELOGINREQUEST_H
#define MESSAGELOGINREQUEST_H

#include <cJSON.h>


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
*/

#ifndef MESSAGELOGINREQUEST_H
#define MESSAGELOGINREQUEST_H

#include <string>

class MessageLoginRequest {

public:
    MessageLoginRequest(const std::string& email, const std::string& password,
                        const std::string& deviceUuid, const std::string& deviceToken);

    std::string toJson() const;

private:
    std::string email;
    std::string password;
    std::string deviceUuid;
    std::string deviceToken;
};

#endif
