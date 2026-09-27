#include "CommonSetup.h"

#include <memory>

#include "esp_log.h"
#include "esp_task_wdt.h"

#include "AuthenticationController.h"
#include "CurrentDateTimeController.h"
#include "Sensors/SensorsController.h"
#include "Storage.h"
#include "Tasks/TaskController.h"
#include "UserCredentials.h"
#include "comm/BleChannel.h"
#include "comm/MessageBus.h"
#include "comm/RemoteChannel.h"
#include "telemetry/Uplink.h"

static const char* TAG = "CommonSetup";

namespace {

UserCredentials credentialsFromConfig(const croniot::CroniotConfig& cfg) {
    return UserCredentials(
        cfg.accountEmail,
        cfg.accountUuid,
        cfg.accountPassword,
        cfg.deviceUuid,
        "",
        cfg.deviceName,
        cfg.deviceDescription
    );
}

void persistCredentialsIfChanged(const UserCredentials& desired) {
    UserCredentials inMemory = Storage::instance().readUserCredentials();

    if (inMemory.accountEmail    != desired.accountEmail ||
        inMemory.accountUuid     != desired.accountUuid ||
        inMemory.accountPassword != desired.accountPassword ||
        inMemory.deviceUuid      != desired.deviceUuid ||
        inMemory.deviceName      != desired.deviceName) {
        ESP_LOGI(TAG, "Storing new credentials");
        Storage::instance().saveUserCredentials(desired);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

}

bool CommonSetup::setup(const croniot::CroniotConfig& config) {
    using namespace croniot;

    persistCredentialsIfChanged(credentialsFromConfig(config));

    auto& bus = MessageBus::instance();
    bus.setDeviceUuid(config.deviceUuid);

    for (auto type : config.channels) {
        switch (type) {
            case ChannelType::Remote:
                bus.addChannel(std::make_unique<RemoteChannel>(config.remote));
                break;
            case ChannelType::Ble:
                bus.addChannel(std::make_unique<BleChannel>(config.deviceUuid, config.ble));
                break;
        }
    }

    return bus.startConnection([this]() {
        xTaskCreate(
            CommonSetup::authenticateWithServerTask,
            "authenticateWithServerTask",
            16384,
            this,
            1,
            &this->authenticateWithServerTaskTaskHandle
        );
    });
}

void CommonSetup::authenticateWithServerTask(void* pvParameters) {
    bool authenticated;
    if (croniot::MessageBus::instance().hasServerAuthChannel()) {
        authenticated = AuthenticationController::instance().init();
    } else {
        authenticated = true;
        ESP_LOGI(TAG, "No server-auth channel, skipping server authentication");
    }
    ESP_LOGI(TAG, "\n\n\n###AUTHENTICATED WITH SERVER: %s", authenticated ? "true" : "false");

    CurrentDateTimeController::instance().run();

    if (authenticated) {
        if (croniot::MessageBus::instance().startMessaging()) {
            SensorsController::instance().init();
            TaskController::instance().init();
            // Only from here on does MessageBus actually have a
            // connected channel to drain through - croniot::log::init()
            // (main.cpp's first line) installs the hook and mounts the
            // Journal long before this, on purpose, so nothing captured
            // before authentication is lost (plan §10's two-step init).
            croniot::telemetry::Uplink::instance().start();
        } else {
            ESP_LOGE(TAG, "Could not start messaging channel");
        }
    }

    ESP_LOGI(TAG, "Authentication task completed, freeing resources");
    vTaskDelete(NULL);
}
