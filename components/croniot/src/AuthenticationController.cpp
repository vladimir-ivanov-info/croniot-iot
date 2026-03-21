#include "AuthenticationController.h"

bool AuthenticationController::init() {
    bool authenticationResult = false;
    bool forceRegisterDevice = true;

    UserCredentials credentials = Storage::instance().readUserCredentials();

    ESP_LOGI("Auth", "Auth %s", credentials.deviceToken.c_str());

    
    if (!forceRegisterDevice &&
        !credentials.accountEmail.empty() &&
        !credentials.accountPassword.empty() &&
        !credentials.deviceUuid.empty() &&
        !credentials.deviceToken.empty()) {

        ESP_LOGI("Auth", "Credentials found, trying to log in...");
        Result loginResult = login(credentials);
        ESP_LOGI("Auth", "Login result: %s %s", loginResult.success ? "true" : "false", loginResult.message.c_str());

        while (!loginResult.success && loginResult.message == "-1") {
            loginResult = login(credentials);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }

        registerSensorTypes();
        registerTasks();
        authenticationResult = true;

    } else {
        ESP_LOGI("Auth", "Stack high watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
        ESP_LOGI("Auth", "Credentialszzzz not found, trying to register device...");

        Result resultRegisterDevice = registerDevice();
        ESP_LOGI("Auth", "Register device result: success=%d, message=%s", resultRegisterDevice.success, resultRegisterDevice.message.c_str());

        ESP_LOGI("Stack", "Stack high watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));

        if (resultRegisterDevice.success) {
            registerSensorTypes();
            registerTasks();
            authenticationResult = true;
        }
    }

    return authenticationResult;
}

Result AuthenticationController::login(UserCredentials credentials) {
    MessageLoginRequest messageLogin(credentials.accountEmail, credentials.accountPassword, credentials.deviceUuid, credentials.deviceToken);
    std::string jsonPayload = messageLogin.toJson();
    return HttpProvider::get()->sendHttpPost(jsonPayload, ROUTE_IOT_LOGIN);
}

Result AuthenticationController::registerDevice() {
    UserCredentials credentials = Storage::instance().readUserCredentials();
    MessageRegisterDevice message(credentials.accountEmail, credentials.accountPassword, credentials.deviceUuid,
                                   credentials.deviceName, credentials.deviceDescription);

    std::string jsonPayload = message.toJson();
    std::string serverAddress = NetworkManager::instance().serverAddress;
    std::string serverPort = std::to_string(NetworkManager::instance().serverPort);

    std::string serverUrl = "http://" + serverAddress + ":" + serverPort + ROUTE_REGISTER_CLIENT;
    ESP_LOGI("Auth", "Register device query to: %s", serverUrl.c_str());

    Result result = HttpProvider::get()->sendHttpPost(jsonPayload, ROUTE_REGISTER_CLIENT);
    if (result.success) {
        UserCredentials newCreds(credentials.accountEmail, credentials.accountUuid, credentials.accountPassword,
                                 credentials.deviceUuid, result.message, credentials.deviceName, credentials.deviceDescription);

        Storage::instance().saveUserCredentials(newCreds);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    } else {
        ESP_LOGE("Auth", "HTTP Request failed. Server online? Error: %s", result.message.c_str());
    }

    return result;
}

void AuthenticationController::registerSensorTypes() {
    UserCredentials credentials = Storage::instance().readUserCredentials();
    auto sensorTypes = SensorsController::instance().getAllSensorTypes();

    for (auto* sensorType : sensorTypes) {
        MessageRegisterSensorType msg(credentials.deviceUuid, credentials.deviceToken, *sensorType);
        std::string json = msg.toJsonString();
        
        Result result = HttpProvider::get()->sendHttpPost(json, ROUTE_REGISTER_SENSOR_TYPE);
        ESP_LOGI("Auth", "Register sensor response: %s", result.toString().c_str());
    }
}

void AuthenticationController::registerTasks() {
    UserCredentials credentials = Storage::instance().readUserCredentials();
    auto taskTypes = TaskController::instance().getAllTaskTypes();

    for (const auto& taskType : taskTypes) {
        MessageRegisterTaskType msg(credentials.deviceUuid, credentials.deviceToken, taskType);
        std::string json = msg.toJsonString();

        if (DEBUG_REGISTER_TASKS)
            ESP_LOGI("Auth", "Registering task: %s", json.c_str());

        Result result = HttpProvider::get()->sendHttpPost(json, ROUTE_REGISTER_TASK_TYPE);

        if (DEBUG_REGISTER_TASKS)
            ESP_LOGI("Auth", "Register task response: %s", result.toString().c_str());
    }
}

Result AuthenticationController::parseResult(const std::string& jsonString) {
    CJsonPtr root(cJSON_Parse(jsonString.c_str()));
    if (!root) {
        return Result(false, "Local cJSON_Parse() failed, JSON:\n" + jsonString);
    }

    cJSON* successItem = cJSON_GetObjectItem(root.get(), "success");
    cJSON* messageItem = cJSON_GetObjectItem(root.get(), "message");

    bool success = cJSON_IsTrue(successItem);
    std::string message = messageItem ? messageItem->valuestring : "";

    return Result(success, message);
}

void AuthenticationController::registerSensor() {
    // Implementar si es necesario
}

void AuthenticationController::uninit() {
    // MQTT y limpieza si hace falta
}
