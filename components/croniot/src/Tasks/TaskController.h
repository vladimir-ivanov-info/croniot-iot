// ESP-IDF version of TaskController (migrated)
#ifndef TASKCONTROLLER_H
#define TASKCONTROLLER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <list>
#include <string>
#include <map>

#include "esp_log.h"
#include "TaskType.h"
#include "TaskBase.h"
#include "CurrentDateTimeController.h"
#include "Storage.h"
#include "network/mqtt/MqttProvider.h"
#include "SimpleTaskData.h"
#include "TaskProgressUpdate.h"
#include "CommonConstants.h"

class TaskController {
public:
    static TaskController& instance() {
        static TaskController* _instance = nullptr;
        if (_instance == nullptr) {
            _instance = new TaskController();
        }
        return *_instance;
    }

    TaskController(){}

    void addTaskType(TaskType taskType) { taskTypes.push_back(taskType); }
    std::list<TaskType> getAllTaskTypes() { return taskTypes; }

    void addTask(TaskBase* task) { tasks.push_back(task); }
    void init();

    void registerCallback(const std::string& deviceUuid, int taskTypeUid, TaskBase* taskInstance);
    void processMessage(int taskTypeUid, const std::string& message, unsigned int length); //TODO length necessary?
    void processMessageTaskData(int taskTypeUid, const std::string& message, unsigned int length);
    void processMessageTaskStateInfoSync(int taskTypeUid, const std::string& message, unsigned int length);

    void enqueueTaskProgressUpdate(TaskProgressUpdate& taskProgressUpdate);

private:
    std::list<TaskType> taskTypes;
    std::list<TaskBase*> tasks;

    static void taskFunction(void* pvParameters);
    TaskHandle_t taskHandle;

    static void taskProgressUpdateFunction(void* pvParameters);
    TaskHandle_t taskProgressUpdateHandle;

    TaskData processMessage(const std::string& message);

    static std::string byteArrayToString(uint8_t* data, unsigned int length);


    std::map<int, TaskBase*> tasksMap;
        std::map<int, TaskBase*> taskStateInfoSyncMap;

    std::map<int, std::string> stringToMap(const std::string& input);



    QueueHandle_t progressUpdateQueue;

    void runTaskRightNow(TaskData& taskData);


};

#endif
