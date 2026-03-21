#ifndef MESSAGEREGISTERTASKTYPE_H
#define MESSAGEREGISTERTASKTYPE_H

#include "Tasks/TaskType.h"
#include "cJSON.h"
#include <string>

class MessageRegisterTaskType {
public:
    MessageRegisterTaskType(const std::string& deviceUuid, const std::string& deviceToken, const TaskType& taskType);

    cJSON* toJson(cJSON* root) const;
    
    std::string toJsonString() const;

private:
    std::string deviceUuid;
    std::string deviceToken;
    TaskType taskType;
};

#endif
