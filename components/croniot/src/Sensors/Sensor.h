#ifndef SENSOR_H
#define SENSOR_H

#include <string>

#include "esp_log.h"
#include "Messages/MessageSensorData.h"
#include "comm/MessageBus.h"

class Sensor {
public:
    virtual void run() = 0;

protected:
    void sendSensorData(int sensorUid, const std::string& sensorValue) {
        MessageSensorData messageSensorData(sensorUid, sensorValue);
        Result result = croniot::MessageBus::instance().publishSensorData(sensorUid, messageSensorData.toString());
        if (!result.success) {
            ESP_LOGW("Sensor", "publishSensorData(%d) failed: %s", sensorUid, result.message.c_str());
        }
    }
};

#endif
