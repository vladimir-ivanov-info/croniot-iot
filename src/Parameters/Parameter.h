#ifndef PARAMETER_H
#define PARAMETER_H

#include <Arduino.h>
#include <iostream>
#include <map>

#define PARAMETERS_NUMBER "number"
#define PARAMETERS_STRING "string"
#define PARAMETERS_TIME "time"

#define PARAMETERS_NUMBER_MIN_VALUE "minValue"
#define PARAMETERS_NUMBER_MAX_VALUE "maxValue"

#include <ArduinoJson.h>

class Parameter{
    public:
        Parameter(int uid, String name, String type, String description, String unit, std::map<String, String> constraints) : uid(uid), name(name), type(type), description(description), unit(unit), constraints(constraints){}

        int uid;
        String name;
        String unit;
        String type;
        String description;
        std::map<String, String> constraints;

        String toJson();

    private:

};

#endif