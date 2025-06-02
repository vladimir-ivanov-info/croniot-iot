#ifndef SENSOR_H
#define SENSOR_H

#include "Messages/MessageSensorData.h"
#include "network/mqtt/MqttProvider.h"

//TODO rename to SensorProcess or SensorTask
class Sensor {

    public:
        
        virtual void run() = 0;

    protected:

        //TODO delegate this method to another class. SensorController, for example
        void sendSensorData(String deviceUuid, int sensorUid, String sensorValue){
            MessageSensorData messageSensorData(sensorUid, sensorValue);
            String topicSensorData = "/" + deviceUuid + "/sensor_data";
            MqttProvider::get()->publish(topicSensorData.c_str(), messageSensorData.toString().c_str());
        };

};


#endif