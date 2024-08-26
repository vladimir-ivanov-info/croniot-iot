#ifndef MESSAGEREGISTERTASKTYPE_H
#define MESSAGEREGISTERTASKTYPE_H

#include "ArduinoJson.h"
#include "Global.h"
#include <Arduino.h>
#include "Tasks/TaskType.h"

class MessageRegisterTaskType{

    public:
        MessageRegisterTaskType(String deviceUuid, String deviceToken, TaskType taskType) : deviceUuid(deviceUuid), deviceToken(deviceToken), taskType(taskType){};
        JsonObject toJson(JsonObject root);

    private:
        String deviceUuid;
        String deviceToken;
        TaskType taskType;
};

#endif