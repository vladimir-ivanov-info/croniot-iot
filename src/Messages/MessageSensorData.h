#ifndef MESSAGESENSORDATA_H
#define MESSAGESENSORDATA_H

#include <ArduinoJson.h>

class MessageSensorData{

    public:

        MessageSensorData(int id, String value) : id(id), value(value){}
        String toString();

    private:
        String messageName = "sensor_data";
        int id;
        String value;

};

#endif