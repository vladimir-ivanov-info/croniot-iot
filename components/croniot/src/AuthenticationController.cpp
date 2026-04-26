#include "AuthenticationController.h"

#include "comm/MessageBus.h"
#include "esp_log.h"

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
        ESP_LOGI("Auth", "Credentials not found, trying to register device...");

        Result resultRegisterDevice = registerDevice();
        ESP_LOGI("Auth", "Register device result: success=%d, message=%s", resultRegisterDevice.success, resultRegisterDevice.message.c_str());

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
    return croniot::MessageBus::instance().login(messageLogin.toJson());
}

Result AuthenticationController::registerDevice() {
    UserCredentials credentials = Storage::instance().readUserCredentials();
    MessageRegisterDevice message(credentials.accountEmail, credentials.accountPassword, credentials.deviceUuid,
                                   credentials.deviceName, credentials.deviceDescription);

    Result result = croniot::MessageBus::instance().registerDevice(message.toJson());
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
        Result result = croniot::MessageBus::instance().registerSensorType(msg.toJsonString());
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

        Result result = croniot::MessageBus::instance().registerTaskType(json);

        if (DEBUG_REGISTER_TASKS)
            ESP_LOGI("Auth", "Register task response: %s", result.toString().c_str());
    }
}

Result AuthenticationController::sendHttpPost(std::string content, std::string route) {
    return Result(false, "sendHttpPost is deprecated; use MessageBus directly");
}

Result AuthenticationController::parseResult(const std::string& jsonString) {
    return Result(false, "parseResult moved to HttpController::parseResult");
}

void AuthenticationController::registerSensor() {}

void AuthenticationController::uninit() {}
