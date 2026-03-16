// #ifndef SENSORTYPE_H
// #define SENSORTYPE_H

// #include <list>
// #include "Parameters/Parameter.h"

// using namespace std;

// class SensorType{

//     public:
//         SensorType(String uid, String name, String description, Parameter parameter);
//         SensorType(String uid, String name, String description, list<Parameter> parameters) : uid(uid), name(name), description(description), parameters(parameters){}

//         JsonObject toJson(JsonObject root);

//     private:
//         String uid;
//         String name;
//         String description;
//         list<Parameter> parameters;

// };

// #endif

#ifndef SENSORTYPE_H
#define SENSORTYPE_H

#include <list>
#include <string>
#include "Parameters/Parameter.h"
#include "cJSON.h"



class SensorType {
public:
    SensorType(const std::string& uid, const std::string& name, const std::string& description, const Parameter& parameter);
    SensorType(const std::string& uid, const std::string& name, const std::string& description, const std::list<Parameter>& parameters);

    cJSON* toJson(cJSON* root) const;

private:
    std::string uid;
    std::string name;
    std::string description;
    std::list<Parameter> parameters;
};

#endif
