#ifndef SENSORSCONTROLLER_H
#define SENSORSCONTROLLER_H

#include <stdio.h>
#include <map>

#include "SensorType.h"
//#include "SensorsDefs.h"

#include "MQTTManager.h"
/*
#include "SensorTaskTemperatureCPU.h"
#include "SensorWiFiStrength.h"
#include "SensorBatteryVoltage.h"
#include "SensorBattery.h"
#include "SensorSolarPanel.h"

#include "Battery/SensorBattery2.h"*/

#include "Messages/MessageSensorData.h"

#include "Sensor.h"


class SensorsController{

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
       //void updateSensor(int sensorId, String newValue);

    private:

        list<SensorType*> sensorTypes;
        list<Sensor*> sensors;

        //SensorWiFiStrength *sensorWifiSignal;
       // SensorBattery *sensorBattery;

       // std::map<int, SensorType> sensors;

        TaskHandle_t taskHandle = NULL;

        // Static function for the RTOS task
        static void taskFunction(void *param) {
            SensorsController *self = (SensorsController *)param;



            //int sensorId = static_cast<int>(SENSOR_TEMPERATURE_CPU);

            // Your RTOS task logic goes here
            for (;;) {
                // Do something
                //float temperature = (temprature_sens_read() - 32) / 1.8;
                //String temperatureStr = String(temperature);

                //SensorsController::instance().updateSensor(sensorId, temperatureStr);

                vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1000ms

                //self->publish();
               // self->getClient().flush();
               // self->getClient().loop();
                
            }
        }

};

#endif