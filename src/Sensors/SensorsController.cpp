#include "SensorsController.h"

void SensorsController::init(){
/*
    Sensor sensorTemperatureCpu(SENSOR_TEMPERATURE_CPU);
    Sensor sensorWifiStrength(SENSOR_WIFI_STRENGTH);
    Sensor sensorVoltageMain(SENSOR_VOLTAGE_MAIN);

    */

    /*xTaskCreatePinnedToCore(
        taskFunction,      // Function to run the task
        "MyTask",          // Task name
        10000,             // Stack size (words)
        this,              // Task parameter (pass the instance of the singleton)
        1,                 // Task priority
        &taskHandle,       // Task handle
        1                  // Core to run the task (0 or 1)
    );*/

/*
    //SensorTaskTemperatureCPU* task1 = new SensorTaskTemperatureCPU();
    //task1->run();

    sensorWifiSignal = new SensorWiFiStrength();
    sensorWifiSignal->run();

    //SensorBatteryVoltage *task2 = new SensorBatteryVoltage();
    //task2->run();

   // sensorBattery = new SensorBattery();
   // sensorBattery->run();

    SensorBattery2 *sensorBattery2 = new SensorBattery2();
    sensorBattery2->run(); 

   // SensorSolarPanel *sensor4 = new SensorSolarPanel();
   // sensor4->run();
*/
}

void SensorsController::uninit(){
   // sensorWifiSignal->stop();
   // sensorBattery->stop();
}