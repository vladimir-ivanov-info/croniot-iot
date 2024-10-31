#ifndef TASKDATA_H
#define TASKDATA_H

#include <map>
#include <Arduino.h>

class TaskData {

    public:
        int taskTypeUid;
        int taskUid;
        std::map<int, String> parametersValues;

};

#endif