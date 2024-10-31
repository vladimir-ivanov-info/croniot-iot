#ifndef HTTPCONTROLLER_H
#define HTTPCONTROLLER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "Result.h"

class NetworkManager;  // Forward declaration

class HttpController{

    public:

        static HttpController& instance() {
            static HttpController _instance;  // No need to use a pointer
            return _instance;
        }

        Result sendHttpPost(String content, String route);

    private:
        Result parseResult(const String& jsonString);

};

#endif