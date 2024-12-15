#ifndef SENSORTYPE_H
#define SENSORTYPE_H

#include <list>
#include "Parameters/Parameter.h"

using namespace std;

class SensorType{

    public:
        SensorType(String uid, String name, String description, Parameter parameter);
        SensorType(String uid, String name, String description, list<Parameter> parameters) : uid(uid), name(name), description(description), parameters(parameters){}

        JsonObject toJson(JsonObject root);

    private:
        String uid;
        String name;
        String description;
        list<Parameter> parameters;

};

#endif