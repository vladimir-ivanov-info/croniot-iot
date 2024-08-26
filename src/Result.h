#ifndef RESULT_H
#define RESULT_H

#include <Arduino.h>
#include "ArduinoJson.h"

class Result{

    public:
        Result(){}
        Result(bool success, String message) : success(success), message(message){}

        bool success;
        String message;

        String toString();

    private:


};

#endif