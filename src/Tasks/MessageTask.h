#ifndef MESSAGETASK_H
#define MESSAGETASK_H

#include <Arduino.h>
#include <map>

class MessageTask{

    public:
        MessageTask(){};

    private:
        int taskTypeUid;
        std::map<int, String> parametersValues;
        int taskUid;
};

#endif