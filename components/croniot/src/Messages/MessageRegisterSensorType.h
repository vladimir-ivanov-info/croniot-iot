/*#ifndef MESSAGEREGISTERSENSORTYPE_H
#define MESSAGEREGISTERSENSORTYPE_H

#include "Parameters/Parameter.h"
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

#endif*/

#ifndef MESSAGEREGISTERSENSORTYPE_H
#define MESSAGEREGISTERSENSORTYPE_H

#include <string>
#include "Parameters/Parameter.h"
#include "Sensors/SensorType.h"

#include "cJSON.h"

class MessageRegisterSensorType {

public:
    MessageRegisterSensorType(const std::string& deviceUuid,
                              const std::string& deviceToken,
                              const SensorType& sensor);

    cJSON* toJson() const;


    std::string toJsonString() const;

private:
    std::string deviceUuid;
    std::string deviceToken;
    SensorType sensor;
};

#endif
