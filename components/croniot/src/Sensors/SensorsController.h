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
        std::list<SensorType*> getAllSensorTypes(){ return sensorTypes; }

        void addSensor(Sensor *sensor){ sensors.push_back(sensor); }

        void init(); //initializes the map of sensors and runs each sensor's task //TODO rename to "runSensorTasks"
        void uninit();

    private:
        // Drives Sensor::maybeFlush() for every registered sensor -
        // plan §7.2/§12.6: a sensor buffering under ReportPolicy::Batch
        // needs *something* checking "is it time yet" periodically,
        // since SensorReportBuffer itself has no timer (same explicit-
        // nowMs determinism as LevelResolver/JournalCursor elsewhere in
        // this SDK - see SensorReportBuffer.h). A sensor that never
        // called setReporting() has nothing buffered, so this is a
        // no-op sweep for it every tick.
        static void flushTask(void* arg);

        std::list<SensorType*> sensorTypes;
        std::list<Sensor*> sensors;

};

#endif
