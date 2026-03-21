#ifndef TASKTYPE_H
#define TASKTYPE_H

#include <string>
#include <list>
#include "../Parameters/Parameter.h"

extern "C" {
    #include "cJSON.h"
}

class TaskType {
public:
    TaskType() = default;

    TaskType(int uid, const std::string& name, const std::string& description, const std::list<Parameter>& parameters)
        : uid(uid), name(name), description(description), parameters(parameters) {}

    TaskType(int uid, const std::string& name, const std::string& description, const Parameter& parameter)
        : uid(uid), name(name), description(description) {
        parameters.push_back(parameter);
    }

    cJSON* toJson(cJSON* root) const;

private:
    int uid;
    std::string name;
    std::string description;
    std::list<Parameter> parameters;
};

#endif
