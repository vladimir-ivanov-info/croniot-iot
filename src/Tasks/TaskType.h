#ifndef TASKTYPE_H
#define TASKTYPE_H

#include "Arduino.h"
#include "Parameter.h"
#include <list>

using namespace std;

class TaskType{

    public:
        TaskType(){}
        TaskType(int uid, String name, String description, list<Parameter> parameters) : uid(uid), name(name), description(description), parameters(parameters){};
        
        JsonObject toJson(JsonObject root);
        
    private:
        int uid;
        String name;
        String description;
        list<Parameter> parameters;
};


#endif