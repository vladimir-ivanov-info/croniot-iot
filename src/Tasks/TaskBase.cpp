#include "TaskBase.h"

TaskBase::TaskBase(const char* name, uint32_t stackSize, UBaseType_t priority, UBaseType_t core) {
    messageQueue = xQueueCreate(10, sizeof(SimpleTaskData)); // Create a queue to hold messages
    if (messageQueue == NULL) {
        Serial.println("Failed to create message queue.");
    } else {
        Serial.println("Message queue created successfully.");
    }

    xTaskCreatePinnedToCore(taskFunction, name, stackSize, this, priority, &taskHandle, core);
}

TaskBase::~TaskBase() {
    if (messageQueue != NULL) {
        vQueueDelete(messageQueue);
    }
}


void TaskBase::enqueueMessage(SimpleTaskData& taskData){
    if (messageQueue == NULL) {
        Serial.println("Message queue is NULL. Cannot enqueue message.");
        return;
    }

    // Dynamically allocate a copy of TaskData
    SimpleTaskData* taskDataCopy = new SimpleTaskData(taskData);
    if (taskDataCopy == NULL) {
        Serial.println("Failed to allocate memory for TaskData.");
        return;
    }

    // Enqueue the pointer to TaskData
    if (xQueueSend(messageQueue, &taskDataCopy, portMAX_DELAY) != pdPASS) {
        Serial.println("Failed to enqueue TaskData.");
        delete taskDataCopy; // Free the allocated memory if enqueue fails
    }
}


void TaskBase::taskFunction(void* pvParameters) {
    TaskBase* taskInstance = static_cast<TaskBase*>(pvParameters);

    while (true) {
        taskInstance->loop();
    }
}

TaskData TaskBase::processMessage(String taskDataJson){

    TaskData taskData;

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, taskDataJson);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        // Return a default Result object with success as false and empty message
        //return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
    } else {
        //int taskTypeUid = doc["taskTypeUid"];
        int taskUid = doc["taskUid"];

        std::map<int, String> parametersValuesForTask;

        JsonObject parametersValues = doc["parametersValues"].as<JsonObject>();

        Serial.println();
        //Serial.print("TaskType uid: "); Serial.println(taskTypeUid);
        Serial.print("Task uid: "); Serial.println(taskUid);
        for (JsonPair kv : parametersValues) {
            const char* key = kv.key().c_str();
            int intKey = atoi(key); // Convert the key to an integer
            String value = kv.value().as<String>();
            Serial.print("Parameter: ["); Serial.print(intKey);  Serial.print("]");
            Serial.print(" value: [" + value); Serial.print("]");
            Serial.println();

            parametersValuesForTask.insert(std::make_pair(intKey, value));
        }

        taskData.taskUid = taskUid;
        taskData.parametersValues = parametersValuesForTask;
    }

    return taskData;
}

