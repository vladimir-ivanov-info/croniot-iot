#ifndef TASKTYPE_H
#define TASKTYPE_H

#include "Arduino.h"
#include "Parameters/Parameter.h"
#include <list>

using namespace std;

class TaskType{

    public:
        TaskType(){}
        TaskType(int uid, String name, String description, list<Parameter> parameters) : uid(uid), name(name), description(description), parameters(parameters){};
        
        TaskType(int uid, String name, String description, Parameter parameter) : uid(uid), name(name), description(description){
            parameters.push_back(parameter);
        };
        
        JsonObject toJson(JsonObject root);
        
    private:
        int uid;
        String name;
        String description;
        list<Parameter> parameters;
};


#endif