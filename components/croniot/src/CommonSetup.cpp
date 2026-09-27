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
#include "health/Health.h"
#include "log/Log.h"
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

    // Deliberately not gated on server auth / a connected channel (unlike
    // Uplink::start() below) - a health report that never leaves the
    // Journal is still useful (readable later, e.g. over BLE), and "this
    // device never got online" is itself the kind of thing worth
    // reporting, not a reason to withhold the report.
    croniot::health::Health::instance().start();

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

            // Remote log-level control (plan §11.5/§12.5 PR15's device
            // side): the server publishes retained JSON on
            // /server/<uuid>/log_config, this device applies it via the
            // exact same LevelResolver "remote" tier setLevel()/init()
            // already feed. Subscribed here, not from Log::init(), for
            // the same reason as Uplink::start() above.
            croniot::MessageBus::instance().subscribeLogConfig(
                [](const std::string& json) { croniot::log::applyRemoteConfig(json); });
        } else {
            ESP_LOGE(TAG, "Could not start messaging channel");
        }
    }

    ESP_LOGI(TAG, "Authentication task completed, freeing resources");
    vTaskDelete(NULL);
}
