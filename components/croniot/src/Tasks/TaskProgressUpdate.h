#pragma once  // Al principio de TaskProgressUpdate.h


#ifndef TASKPROGRESSUPDATE_H
#define TASKPROGRESSUPDATE_H


#include <string>

class TaskProgressUpdate {
public:
    TaskProgressUpdate() = default;

    TaskProgressUpdate(int taskTypeUid, int taskUid, const std::string& state, float progress, const std::string& errorMessage)
        : taskTypeUid(taskTypeUid), taskUid(taskUid), state(state), progress(progress), errorMessage(errorMessage) {}

    int taskTypeUid;
    int taskUid;
    std::string state;
    float progress;
    std::string errorMessage;

    std::string toJson() const;
};

#endif // TASKPROGRESSUPDATE_H
