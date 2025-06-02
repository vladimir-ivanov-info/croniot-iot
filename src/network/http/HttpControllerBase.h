#ifndef HTTPCONTROLLERBASE_H
#define HTTPCONTROLLERBASE_H

#include "Result.h"

class HttpControllerBase{
    
    public:
        virtual bool init() = 0;


        Result parseResult(const String& jsonString) {

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonString);
          
            if (error) {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error.f_str());
              return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
            }
          
            bool success = doc["success"];
            String message = doc["message"];
          
            return Result(success, message);
          }

    private:


};

#endif