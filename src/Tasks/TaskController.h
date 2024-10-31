#ifndef TASKCONTROLLER_H
#define TASKCONTROLLER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <stdio.h>
#include "TaskType.h"
#include "TaskBase.h"
#include "CurrentDateTimeController.h"

#include "Storage.h"

#include "MQTTManager.h"

#include "SimpleTaskData.h"
#include "TaskProgressUpdate.h"

#include "CommonConstants.h"

class TaskController{

    public:

        static TaskController & instance() {
            static  TaskController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new TaskController();
            }
            return *_instance;
        }

        TaskController(){
            progressUpdateQueue = xQueueCreate(20, sizeof(TaskProgressUpdate)); // Create a queue to hold messages
            if (progressUpdateQueue == NULL) {
                Serial.println("Failed to create message queue.");
            } else {
                Serial.println("Message queue created successfully.");
            }
        }

        void addTaskType(TaskType taskType) { taskTypes.push_back(taskType); }
        list<TaskType> getAllTaskTypes(){ return taskTypes; }

        void addTask(TaskBase *task) { tasks.push_back(task); }
        void init();

        void registerCallback(String deviceUuid, int taskTypeUid, TaskBase* taskInstance);

        void processMessage(int taskTypeUid, String message, unsigned int length);

        void enqueueTaskProgressUpdate(TaskProgressUpdate& taskProgressUpdate);

    private:
        list<TaskType> taskTypes;
        list<TaskBase*> tasks;

        static void taskFunction(void* pvParameters);
        TaskHandle_t taskHandle;

        static void taskProgressUpdateFunction(void* pvParameters);
        TaskHandle_t taskProgressUpdateHandle;

        TaskData processMessage(String message); //TODO duplicate of the same method in TaskBase class. Refactor later.

        std::map<int, TaskBase*> tasksMap; // map = { taskTypeUid, TaskBase }

        String byteArrayToString(uint8_t* data, unsigned int length); //TODO duplicate from TaskBase, refactor later

        std::map<int, String> stringToMap(String input);

        QueueHandle_t progressUpdateQueue;

        void runTaskRightNow(TaskData &taskData);

};

#endif