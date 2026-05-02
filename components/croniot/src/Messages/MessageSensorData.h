#ifndef MESSAGESENSORDATA_H
#define MESSAGESENSORDATA_H

#include <string>

extern "C" {
#include "cJSON.h"
}

class MessageSensorData {
public:
    MessageSensorData(int sensorTypeUid, const std::string& value);
    std::string toString() const;

private:
    int sensorTypeUid;
    std::string value;
};

#endif
