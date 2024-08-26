#include "TaskBase.h"

TaskBase::TaskBase(const char* name, uint32_t stackSize, UBaseType_t priority, UBaseType_t core) {
    messageQueue = xQueueCreate(10, sizeof(Message)); // Create a queue to hold messages
    if (messageQueue == NULL) {
        Serial.println("Failed to create message queue.");
    } else {
        Serial.println("Message queue created successfully.");
    }

    xTaskCreatePinnedToCore(taskFunction, name, stackSize, this, priority, &taskHandle, core);

    /*xTaskCreatePinnedToCore(
        taskFunction,       // Function to implement the task
        "TaskBase",         // Name of the task
        4096,               // Stack size in words
        this,               // Task input parameter
        1,                  // Priority of the task
        NULL,               // Task handle
        1                   // Core where the task should run
    );*/
}

TaskBase::~TaskBase() {
    if (messageQueue != NULL) {
        vQueueDelete(messageQueue);
    }
}

void TaskBase::enqueueMessage(const String& topic, byte* payload, unsigned int length) {
    if (messageQueue == NULL) {
        Serial.println("Message queue is NULL. Cannot enqueue message.");
        vPortFree(payload); // Free the payload if enqueue fails
        return;
    }
    Message message;
    message.topic = strdup(topic.c_str());  // Allocate memory and copy topic string
    message.payload = payload;
    message.length = length;
    if (xQueueSend(messageQueue, &message, portMAX_DELAY) != pdPASS) {
        Serial.println("Failed to enqueue message.");
        vPortFree(payload); // Free the payload if enqueue fails
        free(message.topic); // Free the topic string
    }
}

void TaskBase::taskFunction(void* pvParameters) {
    TaskBase* taskInstance = static_cast<TaskBase*>(pvParameters);
    //int iteration = 1;

    while (true) {
        //Serial.print("Iteration: "); Serial.println(iteration);
        taskInstance->loop();
        //Serial.print("End iteration: "); Serial.println(iteration);
        //iteration++;
    }
}

TaskBase::TaskData TaskBase::processMessage(String message){

    TaskData taskData;

    JsonDocument doc;

    // Deserialize the JSON string
    DeserializationError error = deserializeJson(doc, message);

    // Check for errors
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