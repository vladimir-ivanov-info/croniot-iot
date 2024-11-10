#ifndef SIMPLETASKDATA_H
#define SIMPLETASKDATA_H

#include <map>
#include <Arduino.h>

class SimpleTaskData {

    public:
        int taskUid;
        std::map<int, String> parametersValues;

        SimpleTaskData(const int& taskUid, const std::map<int, String>& parametersValues) : taskUid(taskUid), parametersValues(parametersValues) {}
};

#endif