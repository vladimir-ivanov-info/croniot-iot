#ifndef SENSORSCONTROLLER_H
#define SENSORSCONTROLLER_H

#include <stdio.h>
#include <map>
#include "SensorType.h"
#include "Messages/MessageSensorData.h"
#include "Sensor.h"

class SensorsController {

    public:
        static SensorsController & instance() {
            static  SensorsController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new SensorsController();
            }
            return *_instance;
        }

        void addSensorType(SensorType *sensorType){ sensorTypes.push_back(sensorType); }
        list<SensorType*> getAllSensorTypes(){ return sensorTypes; }

        void addSensor(Sensor *sensor){ sensors.push_back(sensor); }

        void init(); //initializes the map of sensors and runs each sensor's task //TODO rename to "runSensorTasks"
        void uninit();

    private:
        list<SensorType*> sensorTypes;
        list<Sensor*> sensors;

};

#endif