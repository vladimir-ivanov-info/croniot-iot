#ifndef MESSAGESENSORDATA_H
#define MESSAGESENSORDATA_H

#include <ArduinoJson.h>

class MessageSensorData{

    public:
        MessageSensorData(int sensorTypeId, String value) : sensorTypeId(sensorTypeId), value(value){}
        String toString();

    private:
        //String messageName = "sensor_data";
        int sensorTypeId;
        String value;

};

#endif