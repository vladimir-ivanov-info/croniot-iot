#ifndef HTTPCONTROLLER_H
#define HTTPCONTROLLER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "Result.h"

#include "NetworkManager.h"

class HttpController{

    public:
        static HttpController & instance() {
            static  HttpController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new HttpController();
            }
            return *_instance;
        }

        Result sendHttpPost(String content, String route);

    private:
        Result parseResult(const String& jsonString);

};

#endif