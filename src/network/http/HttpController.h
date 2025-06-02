#ifndef HTTPCONTROLLER_H
#define HTTPCONTROLLER_H

#include <Arduino.h>
#include "Result.h"

class HttpController {

    public:
        virtual Result sendHttpPost(String content, String route) = 0;

        virtual ~HttpController() = default;

    protected:
        Result parseResult(const String& jsonString) {
            JsonDocument doc;

            DeserializationError error = deserializeJson(doc, jsonString);

            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                // Return a default Result object with success as false and empty message
                return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
            }

            bool success = doc["success"];
            String message = doc["message"];

            return Result(success, message);
        }
};
    

#endif