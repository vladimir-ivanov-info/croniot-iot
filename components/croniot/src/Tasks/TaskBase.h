#ifndef TASKBASE_H
#define TASKBASE_H

#include <string>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_log.h"
#include "SimpleTaskData.h"
#include "TaskData.h"

class TaskBase {

public:
    virtual int getUid() = 0;
    virtual ~TaskBase();

    TaskBase(const char* name, uint32_t stackSize, UBaseType_t priority, UBaseType_t core);
    void enqueueMessage(SimpleTaskData& taskData);

    //void sendCurrentTaskStateInfo();
    std::string getCurrentState() { return currentState; }

    virtual void loop() = 0;
    virtual void run() = 0;

    void init();


protected:
    QueueHandle_t messageQueue;
    //QueueHandle_t messageQueueTaskStateInfoSync;

    std::string currentState = "";

    struct Message {
        char* topic;
        uint8_t* payload;
        unsigned int length;
    };

    virtual void executeTask(SimpleTaskData& taskData) = 0;

    std::string byteArrayToString(uint8_t* data, unsigned int length);

    TaskData processMessage(const std::string& taskDataJson);

private:
    static void taskFunction(void* pvParameters);
    TaskHandle_t taskHandle;
};

#endif
