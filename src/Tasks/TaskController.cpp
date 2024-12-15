#include "TaskController.h"

extern String DEVICE_UUID_EXTERN;


void TaskController::init(){
    for(TaskBase *task : tasks){
        task->run();
    }

    Serial.println("TaskController initialized...");
    xTaskCreatePinnedToCore(taskFunction, "TaskController", 4096, this, 1, &taskHandle, 1);
    xTaskCreatePinnedToCore(taskProgressUpdateFunction, "TaskProgressUpdateFunction", 2*4096, this, 1, &taskProgressUpdateHandle, 1);
}


void TaskController::taskFunction(void* pvParameters) {
    TaskController* taskInstance = static_cast<TaskController*>(pvParameters);

    Serial.println("TaskController task initialized...");

    while (true) {
        vTaskDelay(1*1000 / portTICK_PERIOD_MS);

        std::map<int, String> currentTasks = Storage::instance().getTasksForCurrentDateTime();

        for(auto taskParameters: currentTasks){

            int taskUid = taskParameters.first;

            std::map<int, String> parameters = taskInstance->stringToMap(taskParameters.second);
            SimpleTaskData simpleTaskData(taskUid, parameters);

            Serial.print("Task will be executed now:"); Serial.println(taskUid);

            for(TaskBase *task : taskInstance->tasks){
                if(task->getUid() == taskUid){
                    task->enqueueMessage(simpleTaskData);
                }
            }
        }
    }
}

void TaskController::taskProgressUpdateFunction(void* pvParameters) {
    TaskController* taskInstance = static_cast<TaskController*>(pvParameters);

    while(true){
        TaskProgressUpdate *taskProgressUpdate;
        if(xQueueReceive(taskInstance->progressUpdateQueue, &taskProgressUpdate, portMAX_DELAY) == pdPASS){
            
            int taskUid = taskProgressUpdate->taskUid;

            String topic = "/iot_to_server/task_progress_update/" + DEVICE_UUID_EXTERN; //TODO fix later, parametrize

            String message = taskProgressUpdate->toJson();
            Serial.println("Publishing to " + topic);
            Serial.println(message);
            MQTTManager::instance().publish(topic, message);
        }
    }
}

std::map<int, String> TaskController::stringToMap(String input) {
    std::map<int, String> result;
    int start = 0;

    // Print the entire input string for debugging
    //Serial.println("Input String:");
    //Serial.println(input);

    while (start < input.length()) {
        // Find the index of the next newline
        int end = input.indexOf('\n', start);
        if (end == -1) {
            end = input.length(); // Handle the case where the last line doesn't have a newline
        }

        // Extract the current line
        String line = input.substring(start, end);

        // Print the line to see what is being processed
        Serial.print("Processing line: ");
        Serial.println(line);

        // Find the separator between key and value (assumed to be '=')
        int separatorIndex = line.indexOf('=');
        if (separatorIndex != -1) {
            // Extract key and value
            String keyString = line.substring(0, separatorIndex);
            int key = keyString.toInt(); // Convert key to int

            // Print extracted key
            Serial.print("Extracted key: ");
            Serial.println(keyString);

            // If the key is 0 but the keyString is not "0", skip it
            if (key == 0 && keyString != "0") {
                Serial.println("Skipping invalid key (non-numeric): " + keyString);
            } else {
                String value = line.substring(separatorIndex + 1); // Extract value

                // Print extracted value
                Serial.print("Extracted value: ");
                Serial.println(value);

                // Insert into the map
                result[key] = value;
            }
        } else {
            Serial.println("No '=' found in line, skipping...");
        }

        // Move to the next line
        start = end + 1;
    }

    // Debug the map content after processing
    Serial.println("Map content:");
    for (auto const& pair : result) {
        Serial.print("Key: ");
        Serial.print(pair.first);
        Serial.print(", Value: ");
        Serial.println(pair.second);
    }

    // Print the map size
    Serial.print("Final map size: ");
    Serial.println(result.size());

    return result;
}

TaskData TaskController::processMessage(String message){

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
        int taskTypeUid = doc["taskTypeUid"];

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
        taskData.taskTypeUid = taskTypeUid;
        taskData.taskUid = taskUid;
        taskData.parametersValues = parametersValuesForTask;
    }

    return taskData;
}

void TaskController::registerCallback(String deviceUuid, int taskTypeUid, TaskBase* taskInstance){
    tasksMap[taskTypeUid] = taskInstance;

    MQTTManager::instance().registerCallback("/server/" + String(deviceUuid) + "/task_type/" + String(taskTypeUid), taskInstance);
}

void TaskController::processMessage(int taskTypeUid, String message, unsigned int length){
    Serial.println(message);
    TaskData taskData = processMessage(message);

    TaskProgressUpdate taskProgressUpdate(taskData.taskTypeUid, taskData.taskUid, "RECEIVED", 0.0, ""); 
    TaskController::instance().enqueueTaskProgressUpdate(taskProgressUpdate);

    if(taskData.parametersValues.find(COMMON_TASK_PARAMETER_TIME) != taskData.parametersValues.end()){

        //if time is now:
        String time = taskData.parametersValues[COMMON_TASK_PARAMETER_TIME];
        if(CurrentDateTimeController::instance().isCurrentTime(time)){
            runTaskRightNow(taskData);
        } else {
            Storage::instance().saveFutureTask(taskTypeUid, taskData);
        }
    } else {
        runTaskRightNow(taskData);
    }
}

void TaskController::runTaskRightNow(TaskData &taskData){
    int taskTypeUid = taskData.taskTypeUid;

    SimpleTaskData simpleTaskData(taskData.taskUid, taskData.parametersValues);

    Serial.println("Received task type uid: " + taskTypeUid);

    for(TaskBase *task : tasks){
        Serial.println("TaskType uid:" + task->getUid());
        if(task->getUid() == taskTypeUid){
            //Serial.println("Enqueuing: " + parameters);
            task->enqueueMessage(simpleTaskData);
        }
    }
}



String TaskController::byteArrayToString(uint8_t* data, unsigned int length) {
    // Create a temporary char array to hold the data
    char* temp = new char[length + 1]; // +1 for the null terminator
    memcpy(temp, data, length);
    temp[length] = '\0'; // Null terminate the string

    // Create a String from the char array
    String result = String(temp);

    // Clean up the temporary char array
    delete[] temp;

    return result;
};

void TaskController::enqueueTaskProgressUpdate(TaskProgressUpdate& taskProgressUpdate){
    if (progressUpdateQueue == NULL) {
        Serial.println("Message queue is NULL. Cannot enqueue message.");
        return;
    }

    // Dynamically allocate a copy of TaskData
    TaskProgressUpdate* taskProgressUpdateCopy = new TaskProgressUpdate(taskProgressUpdate);
    if (taskProgressUpdateCopy == NULL) {
        Serial.println("Failed to allocate memory for TaskData.");
        return;
    }

    // Enqueue the pointer to TaskData
    if (xQueueSend(progressUpdateQueue, &taskProgressUpdateCopy, portMAX_DELAY) != pdPASS) {
        Serial.println("Failed to enqueue TaskData.");
        delete taskProgressUpdateCopy; // Free the allocated memory if enqueue fails
    }
}