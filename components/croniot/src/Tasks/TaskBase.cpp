#include "TaskBase.h"
#include <cstring>
#include "CJsonPtr.h"
#include "esp_timer.h"

#define TAG "TaskBase"

TaskBase::TaskBase(const char* name, uint32_t stackSize, UBaseType_t priority, UBaseType_t core) {
   messageQueue = xQueueCreate(10, sizeof(SimpleTaskData*)); //QUEUE SIZE VERY IMPORTANT, IF LOW (10) EVERYTHING WORKS SLOWER
    if (messageQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create message queue.");
    } else {
        ESP_LOGI(TAG, "Message queue created successfully.");
    }
}

void TaskBase::init(){
    xTaskCreate(TaskBase::taskFunction, "taskBase_task", 8192, this, tskIDLE_PRIORITY + 1, &taskHandle);
}

TaskBase::~TaskBase() {
    if (messageQueue != nullptr) {
        vQueueDelete(messageQueue);
    }
}

void TaskBase::enqueueMessage(SimpleTaskData& taskData){
    if (messageQueue == nullptr) {
        ESP_LOGE(TAG, "Message queue is NULL. Cannot enqueue message.");
        return;
    }

    auto* taskDataCopy = new SimpleTaskData(taskData);
    if (taskDataCopy == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for TaskData.");
        return;
    }

    UBaseType_t waiting = uxQueueMessagesWaiting(messageQueue);
    ESP_LOGW(TAG, "TIMING: enqueueMessage at %lld (queue depth: %u)", esp_timer_get_time(), waiting);
    if (xQueueSend(messageQueue, &taskDataCopy, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Queue FULL! depth=%u, dropping message.", waiting);
        delete taskDataCopy;
    }
}


void TaskBase::taskFunction(void* pvParameters) {
    TaskBase* taskInstance = static_cast<TaskBase*>(pvParameters);
    while (true) {
        taskInstance->loop();
    }
}

std::string TaskBase::byteArrayToString(uint8_t* data, unsigned int length) {
    return std::string(reinterpret_cast<char*>(data), length);
}

TaskData TaskBase::processMessage(const std::string& taskDataJson) {
    TaskData taskData;

    CJsonPtr root(cJSON_Parse(taskDataJson.c_str()));
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return taskData;
    }

    cJSON* taskUidNode = cJSON_GetObjectItem(root.get(), "taskUid");
    if (!cJSON_IsNumber(taskUidNode)) {
        ESP_LOGE(TAG, "taskUid not found or not a number");
        return taskData;
    }

    taskData.taskUid = taskUidNode->valueint;

    cJSON* parameters = cJSON_GetObjectItem(root.get(), "parametersValues");
    if (cJSON_IsObject(parameters)) {
        cJSON* param = nullptr;
        cJSON_ArrayForEach(param, parameters) {
            int key = std::atoi(param->string);
            std::string value = param->valuestring ? param->valuestring : "";
            ESP_LOGI(TAG, "Parameter [%d] value: [%s]", key, value.c_str());
            taskData.parametersValues[key] = value;
        }
    }

    return taskData;
}
