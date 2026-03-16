#ifndef AUTHENTICATIONCONTROLLER_H
#define AUTHENTICATIONCONTROLLER_H

#include <string>
#include "Messages/MessageRegisterDevice.h"
#include "Messages/MessageRegisterSensorType.h"
#include "Messages/MessageRegisterTaskType.h"
#include "Messages/MessageLoginRequest.h"
#include "network/mqtt/MqttProvider.h"
#include "Sensors/SensorsController.h"
#include "Tasks/TaskController.h"
#include "network/NetworkManager.h"
#include "network/http/HttpProvider.h"
#include "Storage.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

//#include "esp123_vfs_spiffs.h"

class AuthenticationController {
public:
    static AuthenticationController& instance() {
        static AuthenticationController* _instance = nullptr;
        if (!_instance) {
            _instance = new AuthenticationController();
        }
        return *_instance;
    }

    bool init();
    Result registerDevice();
    void registerSensor();
    void registerSensorTypes();
    void registerTasks();
    Result sendHttpPost(std::string content, std::string route);
    Result login(UserCredentials credentials);
    Result parseResult(const std::string& jsonString);

private:
    const std::string ROUTE_IOT_LOGIN = "/api/iot/login";
    const std::string ROUTE_REGISTER_CLIENT = "/api/register_client";
    const std::string ROUTE_REGISTER_TASK_TYPE = "/api/register_task_type";
    const std::string ROUTE_REGISTER_SENSOR_TYPE = "/api/register_sensor_type";

    const bool DEBUG_REGISTER_TASKS = false;
    bool registered = false;
    bool authentified = false;

    void uninit();
};

#endif