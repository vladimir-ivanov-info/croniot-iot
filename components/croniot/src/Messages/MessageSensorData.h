#ifndef MESSAGESENSORDATA_H
#define MESSAGESENSORDATA_H

#include <string>

extern "C" {
#include "cJSON.h"
}

class MessageSensorData {
public:
    MessageSensorData(int sensorTypeId, const std::string& value);
    std::string toString() const;

private:
    int sensorTypeId;
    std::string value;
};

#endif
