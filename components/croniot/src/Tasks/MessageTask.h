#ifndef MESSAGETASK_H
#define MESSAGETASK_H

#include <string>
#include <map>

class MessageTask {
public:
    MessageTask(int taskTypeUid, int taskUid, std::map<int, std::string> parametersValues)
        : taskTypeUid(taskTypeUid), taskUid(taskUid), parametersValues(parametersValues) {}

    int getTaskTypeUid() const { return taskTypeUid; }
    int getTaskUid() const { return taskUid; }
    const std::map<int, std::string>& getParametersValues() const { return parametersValues; }

private:
    int taskTypeUid;
    std::map<int, std::string> parametersValues;
    int taskUid;
};

#endif
