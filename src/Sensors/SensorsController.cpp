#include "SensorsController.h"

void SensorsController::init(){
    for(Sensor *sensor : sensors){
        sensor->run();
    }
}

void SensorsController::uninit(){
    //TODO
    //sensorWifiSignal->stop();
    //sensorBattery->stop();
}