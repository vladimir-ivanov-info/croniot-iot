#include "CommonSetup.h"

#include <cinttypes>
#include <memory>

#include "esp_log.h"
#include "esp_timer.h"
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
#include "ota/OtaController.h"
#include "telemetry/RetryPolicy.h"
#include "telemetry/Uplink.h"

static const char* TAG = "CommonSetup";

// Retry schedule for registering with the server at boot: 5 s, doubling up
// to 5 min, forever. The server being down when the device boots (a power
// cut where the device comes back first) must not leave the device without
// MQTT until someone reboots it.
static constexpr uint64_t kAuthRetryBaseMs = 5 * 1000;
static constexpr uint64_t kAuthRetryCapMs = 5 * 60 * 1000;

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

    // Before any channel exists: a freshly installed firmware's deadline
    // to prove itself counts from boot, and a rollback is reported even
    // if this boot never gets online.
    croniot::ota::begin(config.remote);

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
    // Registration happens once per boot and everything after it (MQTT,
    // sensors, tasks, uplink) depends on it, so it is retried until it
    // succeeds instead of giving up on the first failure. Meanwhile logs,
    // events and sensor data keep accumulating in the Journal and go out
    // once messaging starts.
    bool authenticated = true;
    if (croniot::MessageBus::instance().hasServerAuthChannel()) {
        for (uint32_t attempt = 1; !(authenticated = AuthenticationController::instance().init()); ++attempt) {
            // Jitter from the clock's low bits, as in Uplink.cpp: enough to
            // keep a fleet that lost power together from retrying in lockstep.
            double jitter = static_cast<double>(esp_timer_get_time() % 1000) / 1000.0 * 0.3;
            uint64_t delayMs = croniot::telemetry::RetryPolicy::nextDelayMs(attempt, kAuthRetryBaseMs,
                                                                             kAuthRetryCapMs, jitter);
            ESP_LOGW(TAG, "Server registration failed (attempt %" PRIu32 "), retrying in %" PRIu64 " s",
                     attempt, delayMs / 1000);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    } else {
        ESP_LOGI(TAG, "No server-auth channel, skipping server authentication");
    }
    ESP_LOGI(TAG, "Authenticated with server: %s", authenticated ? "true" : "false");

    CurrentDateTimeController::instance().run();

    if (authenticated) {
        croniot::ota::onServerAuthenticated();
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
            croniot::MessageBus::instance().subscribeLogConfig([](const std::string& json) {
                Result result = croniot::log::applyRemoteConfig(json);
                if (!result.success) {
                    ESP_LOGW(TAG, "Ignoring remote log_config: %s", result.message.c_str());
                }
            });

            // Firmware update requests (retained /server/<uuid>/ota).
            croniot::ota::listen();
        } else {
            ESP_LOGE(TAG, "Could not start messaging channel");
        }
    }

    ESP_LOGI(TAG, "Authentication task completed, freeing resources");
    vTaskDelete(NULL);
}
