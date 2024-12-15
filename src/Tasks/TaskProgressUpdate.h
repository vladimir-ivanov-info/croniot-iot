#ifndef TASKPROGRESSUPDATE_H
#define TASKPROGRESSUPDATE_H

#include <Arduino.h>
#include "ArduinoJson.h"

class TaskProgressUpdate {

    public:
        TaskProgressUpdate(){};
        
        TaskProgressUpdate(const int tasTypekUid, const int taskUid, const String state, const float progress, const String errorMessage) 
        :
        taskTypeUid(tasTypekUid), taskUid(taskUid), state(state), progress(progress), errorMessage(errorMessage) {}

        

        int taskTypeUid;
        int taskUid;
        String state;
        float progress;
        String errorMessage;

        String toJson();

    private:


};

#endif