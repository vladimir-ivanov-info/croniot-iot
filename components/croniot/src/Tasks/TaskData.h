#ifndef TASKDATA_H
#define TASKDATA_H

#include <map>

class TaskData {

    public:
        int taskTypeUid;
        int taskUid;
        std::map<int, std::string> parametersValues;

};

#endif