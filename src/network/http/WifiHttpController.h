#ifndef WIFIHTTPCONTROLLER_H
#define WIFIHTTPCONTROLLER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "Result.h"

#include "HttpController.h"

class NetworkManager;  // Forward declaration

class WifiHttpController : public HttpController {

    public:

        static WifiHttpController& instance() {
            static WifiHttpController _instance;  // No need to use a pointer
            return _instance;
        }

        Result sendHttpPost(String content, String route) override;

    private:
        //Result parseResult(const String& jsonString);

};

#endif