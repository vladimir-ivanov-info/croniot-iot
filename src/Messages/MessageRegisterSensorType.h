#ifndef MESSAGEREGISTERSENSORTYPE_H
#define MESSAGEREGISTERSENSORTYPE_H

#include "Tasks/Parameter.h"
#include "Sensors/SensorType.h"

class MessageRegisterSensorType{

    public:
        MessageRegisterSensorType(String deviceUuid, String deviceToken, SensorType sensor) : deviceUuid(deviceUuid),  deviceToken(deviceToken), sensor(sensor){} // Constructor

        JsonObject toJson(JsonObject root);

    private:
        String deviceUuid;
        String deviceToken;
        SensorType sensor;
};

#endif