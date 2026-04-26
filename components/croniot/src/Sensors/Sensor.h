#ifndef SENSOR_H
#define SENSOR_H

#include <string>

#include "Messages/MessageSensorData.h"
#include "comm/MessageBus.h"

class Sensor {
public:
    virtual void run() = 0;

protected:
    void sendSensorData(int sensorUid, const std::string& sensorValue) {
        MessageSensorData messageSensorData(sensorUid, sensorValue);
        croniot::MessageBus::instance().publishSensorData(sensorUid, messageSensorData.toString());
    }
};

#endif
