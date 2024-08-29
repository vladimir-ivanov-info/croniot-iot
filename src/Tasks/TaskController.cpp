#include "TaskController.h"

void TaskController::init(){

   // TaskLedTest *task = new TaskLedTest(2, { {1, "10"} });  // Allocate TaskWaterPlants on the heap
   // TaskLedTest *task = new TaskLedTest(2, { {1, "10"} }); // Example parameter values
    

    //vTaskDelay(500 / portTICK_PERIOD_MS);
    //task->init();




//for auto task: run

    //TaskLedTest *task = new TaskLedTest(); // Example parameter values
    //task->run();

    for(TaskBase *task : tasks){
        task->run();
    }

    Serial.println("TaskController initialized...");
        Serial.println(tasks.size());

}