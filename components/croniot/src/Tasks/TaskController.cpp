#include <sstream>  // <--- necesario para std::istringstream
#include <map>
#include <string>
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "TaskController.h"

extern std::string DEVICE_UUID_EXTERN;

void TaskController::init(){

    ESP_LOGE("TaskController", "Tasks total: %d", tasks.size());
    //Important to declare before creating the tasks!
    progressUpdateQueue = xQueueCreate(10, sizeof(TaskProgressUpdate*));
    if (progressUpdateQueue == nullptr) {
        ESP_LOGE("TaskController", "Failed to create progressUpdateQueue");
    }

    
    for(TaskBase *task : tasks){
        task->init();
        ESP_LOGE("TaskController", "init done");

        task->run();
        ESP_LOGE("TaskController", "task->run %d", task->getUid());
    }

   /* progressUpdateQueue = xQueueCreate(10, sizeof(TaskProgressUpdate*));
    if (progressUpdateQueue == nullptr) {
        ESP_LOGE("TaskController", "Failed to create progressUpdateQueue");
    }*/


    //ESTP DEBE IR DESPUES DEL FOR:

    //xTaskCreatePinnedToCore(taskFunction, "TaskController", 4096, this, 1, &taskHandle, 1);
    xTaskCreate(taskFunction, "TaskController", 4096, this, 5, &taskHandle);

    xTaskCreate(taskProgressUpdateFunction, "TaskProgressUpdateFunction", 2*4096, this, 5, &taskProgressUpdateHandle);


    ESP_LOGI("TaskController", "Initialized...");

}

void TaskController::taskFunction(void* pvParameters) {
    TaskController* taskInstance = static_cast<TaskController*>(pvParameters);

    ESP_LOGI("TaskController", "Task loop1 running...");

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        auto currentTasks = Storage::instance().getTasksForCurrentDateTime();

        if(!currentTasks.empty()){
            ESP_LOGW("TaskController", "getTasksForCurrentDateTime returned %d tasks", currentTasks.size());
        }

        for(const auto& taskParameters: currentTasks){
            int taskUid = taskParameters.first;
            auto parameters = taskInstance->stringToMap(taskParameters.second);
            SimpleTaskData simpleTaskData(taskUid, parameters);

            ESP_LOGI("TaskController", "Executing Task UID: %d", taskUid);

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
        TaskProgressUpdate *taskProgressUpdate = nullptr;
        if(xQueueReceive(taskInstance->progressUpdateQueue, &taskProgressUpdate, portMAX_DELAY) == pdPASS && taskProgressUpdate){

            int64_t t0 = esp_timer_get_time();

            std::string topic = "/iot_to_server/task_progress_update/" + DEVICE_UUID_EXTERN;
            std::string message = taskProgressUpdate->toJson();

            int64_t t1 = esp_timer_get_time();
            MqttProvider::get()->publish(topic, message);
            int64_t t2 = esp_timer_get_time();

            ESP_LOGW("TaskController", "TIMING: toJson=%lldus publish=%lldus total=%lldus | %s",
                     (t1-t0), (t2-t1), (t2-t0), message.c_str());

            delete taskProgressUpdate;
        }
    }
}

std::map<int, std::string> TaskController::stringToMap(const std::string& input) {
    std::map<int, std::string> result;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string keyStr = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        //TODO:
        //try {
            int key = std::stoi(keyStr);
            result[key] = value;
        //} catch (...) {
       //     continue;
        //}
    }

    return result;
}

/*TaskData TaskController::processMessage(const std::string& message){
    TaskData taskData;
    JsonDocument doc;

    if (deserializeJson(doc, message).failed()) {
        ESP_LOGE("TaskController", "JSON parse failed");
        return taskData;
    }

    taskData.taskUid = doc["taskUid"];
    taskData.taskTypeUid = doc["taskTypeUid"];

    JsonObject values = doc["parametersValues"].as<JsonObject>();
    for (JsonPair kv : values) {
        int key = atoi(kv.key().c_str());
        std::string value = kv.value().as<std::string>();
        taskData.parametersValues[key] = value;
    }

    return taskData;
}*/



TaskData TaskController::processMessage(const std::string& message) {
    TaskData taskData;

    cJSON* root = cJSON_Parse(message.c_str());
    if (!root) {
        ESP_LOGE("TaskController", "JSON parse failed");
        return taskData;
    }

    // Leer taskUid y taskTypeUid
    cJSON* taskUid = cJSON_GetObjectItem(root, "taskUid");
    cJSON* taskTypeUid = cJSON_GetObjectItem(root, "taskTypeUid");
    if (cJSON_IsNumber(taskUid)) {
        taskData.taskUid = taskUid->valueint;
    }
    if (cJSON_IsNumber(taskTypeUid)) {
        taskData.taskTypeUid = taskTypeUid->valueint;
    }

    // Leer parametersValues
    cJSON* parametersValues = cJSON_GetObjectItem(root, "parametersValues");
    if (cJSON_IsObject(parametersValues)) {
        cJSON* item = parametersValues->child;
        while (item != nullptr) {
            int key = atoi(item->string);
            std::string value = cJSON_IsString(item) ? item->valuestring : "";

            taskData.parametersValues[key] = value;

            item = item->next;
        }
    }

    cJSON_Delete(root);
    return taskData;
}


void TaskController::registerCallback(const std::string& deviceUuid, int taskTypeUid, TaskBase* taskInstance){
    tasksMap[taskTypeUid] = taskInstance;
    std::string topic1 = "/server/" + deviceUuid + "/task_type/" + std::to_string(taskTypeUid);

    //taskStateInfoSyncMap[taskTypeUid] = taskInstance;
    //std::string topictopicTaskStateInfoSyncMap = "/server/" + deviceUuid + "/task_state_info_sync/" + std::to_string(taskTypeUid);

    ESP_LOGI("TaskController", "MQTT listening topic: %s", topic1.c_str());
    MqttProvider::get()->registerCallback(topic1, taskInstance);



    //std::string topic2 = "/server/" + deviceUuid + "/task_state_info_sync"; // + std::to_string(taskTypeUid);
    std::string topic2 = "/server/" + deviceUuid + "/task_state_info_sync/" + std::to_string(taskTypeUid); // + std::to_string(taskTypeUid);
    ESP_LOGI("TaskController", "MQTT listening topic: %s", topic2.c_str());
    MqttProvider::get()->registerCallbackTaskStateInfoSync(topic2, taskInstance);
}


void TaskController::processMessageTaskData(int taskTypeUid, const std::string& message, unsigned int length){
    int64_t tStart = esp_timer_get_time();

    ESP_LOGI("TaskController", "Processing MQTT message: %s", message.c_str());


    TaskData taskData = processMessage(message);
        ESP_LOGI("TaskController", "Processing MQTT message TaskData %d", taskData.taskTypeUid);


    TaskProgressUpdate update(taskData.taskTypeUid, taskData.taskUid, "RECEIVED", 0.0, "");
            ESP_LOGI("TaskController", "Enqueuing MQTT message TaskData");

    int64_t tBeforeEnqueue = esp_timer_get_time();
    enqueueTaskProgressUpdate(update);
    int64_t tAfterEnqueue = esp_timer_get_time();
    ESP_LOGW("TaskController", "TIMING processMessageTaskData: parse=%lldus enqueue=%lldus",
             (tBeforeEnqueue-tStart), (tAfterEnqueue-tBeforeEnqueue));


    for (const auto &par : taskData.parametersValues) {
      //  std::cout << "Clave: " << par.first << " | Valor: " << par.second << std::endl;
        ESP_LOGI("TaskController", " %d  --- %s", par.first, par.second.c_str());
    }

    if(taskData.parametersValues.find(COMMON_TASK_PARAMETER_TIME) != taskData.parametersValues.end()){
        std::string time = taskData.parametersValues[COMMON_TASK_PARAMETER_TIME];
        if(CurrentDateTimeController::instance().isCurrentTime(time)){
            runTaskRightNow(taskData);
        } else {
            Storage::instance().saveFutureTask(taskTypeUid, taskData);
        }
    } else {
        runTaskRightNow(taskData);
    }
}

void TaskController::processMessageTaskStateInfoSync(int taskTypeUid, const std::string& message, unsigned int length){
    ESP_LOGI("TaskController", "Processing MQTT message: %s", message.c_str());
    //TaskData taskData = processMessage(message); qwe

        int taskIndex = 0;
        for(TaskBase *task : tasks){
            //ESP_LOGI("TaskController", "Checking task %d at address: %p", taskIndex, task);
            
            if (task == nullptr) {
                ESP_LOGE("TaskController", "Task %d is NULL!", taskIndex);
                taskIndex++;
                continue;
            }
            
            //ESP_LOGI("TaskController", "Task %d UID: %d", taskIndex, task->getUid());
            
            if(task->getUid() == taskTypeUid){
                //ESP_LOGI("TaskController", "Found matching task %d, sending current state", taskIndex);
                //task->enqueueMessage(simpleData);

                std::string currentState = task->getCurrentState();
                    //ESP_LOGI("TaskController", "Current state: %s", currentState.c_str());

                TaskProgressUpdate taskProgressUpdate(taskTypeUid, -1, currentState, -1.0f, "");
                enqueueTaskProgressUpdate(taskProgressUpdate);

                ESP_LOGI("TaskController", "Message enqueued successfully");
            }
            
            taskIndex++;
        }

}


/*
void TaskController::runTaskRightNow(TaskData& taskData){
    SimpleTaskData simpleData(taskData.taskUid, taskData.parametersValues);

    ESP_LOGI("TaskController", "Run task type UID: %d", taskData.taskTypeUid);

    for(TaskBase *task : tasks){
        if(task->getUid() == taskData.taskTypeUid){
            task->enqueueMessage(simpleData);
        }
    }
}
*/

void TaskController::runTaskRightNow(TaskData& taskData){
    SimpleTaskData simpleData(taskData.taskUid, taskData.parametersValues);

    ESP_LOGI("TaskController", "Run task type UID: %d", taskData.taskTypeUid);
    ESP_LOGI("TaskController", "Number of tasks in list: %zu", tasks.size());

    int taskIndex = 0;
    for(TaskBase *task : tasks){
        //ESP_LOGI("TaskController", "Checking task %d at address: %p", taskIndex, task);
        
        if (task == nullptr) {
            ESP_LOGE("TaskController", "Task %d is NULL!", taskIndex);
            taskIndex++;
            continue;
        }
        
        ESP_LOGI("TaskController", "Task %d UID: %d", taskIndex, task->getUid());
        
        if(task->getUid() == taskData.taskTypeUid){
            ESP_LOGI("TaskController", "Found matching task %d, enqueuing message", taskIndex);
            task->enqueueMessage(simpleData);
            ESP_LOGI("TaskController", "Message enqueued successfully");
        }
        
        taskIndex++;
    }
    
    ESP_LOGI("TaskController", "runTaskRightNow completed");
}


std::string TaskController::byteArrayToString(uint8_t* data, unsigned int length) {
    return std::string(reinterpret_cast<char*>(data), length);
}

void TaskController::enqueueTaskProgressUpdate(TaskProgressUpdate& taskProgressUpdate){
    if (progressUpdateQueue == nullptr) return;

    auto* copy = new TaskProgressUpdate(taskProgressUpdate);
    if (!copy) return;

    if (xQueueSend(progressUpdateQueue, &copy, portMAX_DELAY) != pdPASS) {
        delete copy;
    }
}

