#include "TaskController.h"

void TaskController::init(){
    for(TaskBase *task : tasks){
        task->run();
    }

    Serial.println("TaskController initialized...");
}