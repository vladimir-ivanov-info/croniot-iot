#include "OtaController.h"

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "Ota.h"
#include "OtaManifest.h"
#include "OtaPolicy.h"
#include "Storage.h"
#include "comm/MessageBus.h"
#include "log/Log.h"
#include "network/NetworkManager.h"
#include "telemetry/Uplink.h"

// With CRONIOT_OTA_ENABLE=n the module compiles to the no-op entry
// points at the bottom; these only keep the unused controller building.
#ifndef CONFIG_CRONIOT_OTA_STABLE_SECONDS
#define CONFIG_CRONIOT_OTA_STABLE_SECONDS 60
#define CONFIG_CRONIOT_OTA_CONFIRM_TIMEOUT_MIN 30
#define CONFIG_CRONIOT_OTA_MAX_ATTEMPTS 2
#define CONFIG_CRONIOT_OTA_TASK_STACK_SIZE 8192
#define CONFIG_CRONIOT_OTA_TASK_PRIORITY 1
#endif

#if CONFIG_CRONIOT_OTA_ENABLE && !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#warning "CRONIOT_OTA_ENABLE without BOOTLOADER_APP_ROLLBACK_ENABLE: a new firmware that crashes before confirming itself will not be rolled back automatically"
#endif

namespace croniot::ota {

namespace {

constexpr const char* TAG = "Ota";
constexpr const char* kNvsNamespace = "croniot_ota";
constexpr const char* kNvsKey = "rec";

constexpr size_t kChunkSize = 4096;
constexpr TickType_t kTickPeriod = pdMS_TO_TICKS(5000);
constexpr uint64_t kGateRetryMs = 60 * 1000;
constexpr uint64_t kDownloadRetryMs = 10 * 60 * 1000;
constexpr int kHttpTimeoutMs = 15000;

uint64_t uptimeMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }

struct SelfTest {
    const char* name;
    std::function<bool()> check;
};

std::vector<SelfTest> g_selfTests;
std::function<bool()> g_gate;
std::function<void()> g_beforeRestart;

// Facts the policy needs that only this file can observe.
class Controller {
public:
    void begin(const CroniotConfig::RemoteCfg& remote);
    void listen();
    void onServerAuthenticated() { authenticated_.store(true); }

private:
    static void run(void*);
    void tick();

    void onManifestJson(const std::string& json);
    void handlePendingManifest();
    void validate();
    void probe();

    Result download(const Manifest& m);
    void restart();
    bool serverReachable();

    void load();
    void save();

    bool started_ = false;
    TaskHandle_t task_ = nullptr;
    CroniotConfig::RemoteCfg remote_;
    std::string runningVersion_;
    bool runningPendingVerify_ = false;

    OtaRecord record_;
    BootSituation situation_ = BootSituation::Normal;

    std::atomic<bool> authenticated_{false};
    uint64_t reachableAtMs_ = 0;

    std::optional<bool> selfTestsPassed_;

    std::mutex pendingMutex_;
    std::optional<Manifest> pending_;  // written by the MQTT task, read by croniot_ota
    bool pendingChanged_ = false;
    std::optional<Manifest> current_;  // the request this task is working on
    uint64_t nextAttemptAtMs_ = 0;
};

Controller g_controller;

const uint64_t kStableMs = static_cast<uint64_t>(CONFIG_CRONIOT_OTA_STABLE_SECONDS) * 1000;
const uint64_t kDeadlineMs = static_cast<uint64_t>(CONFIG_CRONIOT_OTA_CONFIRM_TIMEOUT_MIN) * 60 * 1000;
constexpr uint8_t kMaxAttempts = CONFIG_CRONIOT_OTA_MAX_ATTEMPTS;

void Controller::load() {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return;  // first boot: nothing stored
    size_t len = 0;
    if (nvs_get_str(handle, kNvsKey, nullptr, &len) == ESP_OK && len > 0) {
        std::string blob(len, '\0');
        if (nvs_get_str(handle, kNvsKey, blob.data(), &len) == ESP_OK) {
            blob.resize(len > 0 ? len - 1 : 0);  // drop the NUL nvs_get_str counts
            record_ = decodeRecord(blob);
        }
    }
    nvs_close(handle);
}

void Controller::save() {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "could not open NVS namespace '%s'", kNvsNamespace);
        return;
    }
    std::string blob = encodeRecord(record_);
    if (nvs_set_str(handle, kNvsKey, blob.c_str()) != ESP_OK || nvs_commit(handle) != ESP_OK) {
        ESP_LOGE(TAG, "could not persist the update record");
    }
    nvs_close(handle);
}

void Controller::begin(const CroniotConfig::RemoteCfg& remote) {
    if (started_) return;
    started_ = true;
    remote_ = remote;

    const esp_app_desc_t* desc = esp_app_get_description();
    runningVersion_ = desc->version;
    esp_ota_img_states_t state;
    runningPendingVerify_ = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
                            state == ESP_OTA_IMG_PENDING_VERIFY;

    // NVS is normally initialized later (WiFi bring-up); doing it here is
    // the same idempotent idiom as log/BootCounter.cpp.
    esp_err_t nvsResult = nvs_flash_init();
    if (nvsResult == ESP_ERR_NVS_NO_FREE_PAGES || nvsResult == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    load();

    BootOutcome outcome = onBoot(record_, runningVersion_, kMaxAttempts);
    situation_ = outcome.situation;
    if (situation_ == BootSituation::Validating && !runningPendingVerify_) {
        // Rollback disabled in the bootloader, or the image was confirmed
        // already: validate anyway, the checks still decide.
        ESP_LOGW(TAG, "new firmware is not pending verification; automatic rollback unavailable");
    }
    if (situation_ == BootSituation::Normal && runningPendingVerify_) {
        // Flashed over a pending image without going through us (e.g. by
        // cable): nothing to validate against, accept it.
        esp_ota_mark_app_valid_cancel_rollback();
        runningPendingVerify_ = false;
    }

    switch (situation_) {
        case BootSituation::Normal:
            break;
        case BootSituation::Validating:
            log::event("ota_validating", log::Level::Info,
                       {{"version", runningVersion_}, {"from", record_.previousVersion},
                        {"attempt", std::to_string(record_.attempts)}});
            break;
        case BootSituation::RolledBack:
            log::event("ota_rolled_back", log::Level::Warn,
                       {{"version", record_.targetVersion}, {"running", runningVersion_},
                        {"reason", outcome.reason}, {"attempt", std::to_string(record_.attempts)},
                        {"bad", outcome.markedBad ? "true" : "false"}});
            break;
        case BootSituation::Probing:
            log::event("ota_probing", log::Level::Warn,
                       {{"version", record_.targetVersion}, {"running", runningVersion_}});
            break;
    }
    save();

    ESP_LOGI(TAG, "running %s (%s)%s", runningVersion_.c_str(),
             situation_ == BootSituation::Validating ? "validating"
             : situation_ == BootSituation::Probing  ? "probing previous firmware"
                                                     : "normal",
             runningPendingVerify_ ? ", pending verify" : "");

    xTaskCreate(&Controller::run, "croniot_ota", CONFIG_CRONIOT_OTA_TASK_STACK_SIZE, this,
                CONFIG_CRONIOT_OTA_TASK_PRIORITY, &task_);
}

void Controller::listen() {
    MessageBus::instance().subscribeOta([this](const std::string& json) { onManifestJson(json); });
}

// Runs on the MQTT task: parse and hand over, never block here.
void Controller::onManifestJson(const std::string& json) {
    std::optional<Manifest> m = parseManifest(json);
    if (!m && !json.empty()) {
        ESP_LOGW(TAG, "ignoring malformed update request");
        log::event("ota_request_invalid", log::Level::Warn);
    }
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pending_ = std::move(m);
        pendingChanged_ = true;
    }
    if (task_) xTaskNotifyGive(task_);
}

void Controller::run(void* arg) {
    auto* self = static_cast<Controller*>(arg);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, kTickPeriod);
        self->tick();
    }
}

bool Controller::serverReachable() {
    if (reachableAtMs_ != 0) return true;
    if (authenticated_.load() && telemetry::Uplink::instance().acksReceived() > 0) {
        reachableAtMs_ = uptimeMs();
        if (reachableAtMs_ == 0) reachableAtMs_ = 1;
        ESP_LOGI(TAG, "server reachable after %" PRIu64 " s", reachableAtMs_ / 1000);
    }
    return reachableAtMs_ != 0;
}

void Controller::tick() {
    if (situation_ == BootSituation::Validating) validate();
    if (situation_ == BootSituation::Probing) probe();
    handlePendingManifest();
}

void Controller::validate() {
    uint64_t now = uptimeMs();
    if (now >= kStableMs && !selfTestsPassed_) {
        bool ok = true;
        for (const auto& t : g_selfTests) {
            bool passed = t.check ? t.check() : true;
            ESP_LOGI(TAG, "self-test %s: %s", t.name, passed ? "ok" : "FAILED");
            if (!passed) {
                log::event("ota_self_test_failed", log::Level::Error,
                           {{"version", runningVersion_}, {"test", t.name}});
                ok = false;
            }
        }
        selfTestsPassed_ = ok;
    }

    ValidationInputs in;
    in.uptimeMs = now;
    in.selfTestsPassed = selfTestsPassed_.value_or(false);
    in.selfTestsFailed = selfTestsPassed_.has_value() && !*selfTestsPassed_;
    in.serverReachable = serverReachable();

    ValidationVerdict verdict = evaluateValidation(in, kStableMs, kDeadlineMs);
    switch (verdict) {
        case ValidationVerdict::Wait:
            return;
        case ValidationVerdict::Confirm: {
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            uint8_t attempt = record_.attempts;
            recordConfirmed(record_);
            save();
            runningPendingVerify_ = false;
            situation_ = BootSituation::Normal;
            ESP_LOGI(TAG, "firmware %s confirmed (%s)", runningVersion_.c_str(), esp_err_to_name(err));
            log::event("ota_confirmed", log::Level::Info,
                       {{"version", runningVersion_}, {"from", record_.previousVersion},
                        {"attempt", std::to_string(attempt)}});
            return;
        }
        case ValidationVerdict::RollbackSelfTest:
        case ValidationVerdict::RollbackNoConnectivity: {
            const char* reason = verdict == ValidationVerdict::RollbackSelfTest ? kReasonSelfTest
                                                                                 : kReasonNoConnectivity;
            ESP_LOGW(TAG, "firmware %s failed validation (%s), rolling back", runningVersion_.c_str(), reason);
            log::event("ota_validation_failed", log::Level::Error,
                       {{"version", runningVersion_}, {"reason", reason}});
            recordRollback(record_, verdict);
            save();
            vTaskDelay(pdMS_TO_TICKS(2000));  // let the journal pick the event up
            if (g_beforeRestart) g_beforeRestart();
            esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
            // Only returns if there is no previous valid image to go back to.
            ESP_LOGE(TAG, "rollback impossible (%s): keeping %s", esp_err_to_name(err), runningVersion_.c_str());
            recordConfirmed(record_);
            save();
            situation_ = BootSituation::Normal;
            return;
        }
    }
}

void Controller::probe() {
    ProbeVerdict verdict = evaluateProbe(serverReachable() ? reachableAtMs_ : 0, kDeadlineMs);
    if (verdict == ProbeVerdict::Wait) return;
    std::string version = record_.targetVersion;
    bool bad = recordProbeResult(record_, verdict, kMaxAttempts);
    save();
    situation_ = BootSituation::Normal;
    if (verdict == ProbeVerdict::NewVersionGuilty) {
        ESP_LOGW(TAG, "previous firmware reaches the server: %s is to blame", version.c_str());
        log::event("ota_failed", log::Level::Error,
                   {{"version", version}, {"reason", "new_firmware_no_connectivity"}});
    } else {
        ESP_LOGW(TAG, "previous firmware could not reach the server in time either: external outage");
        log::event("ota_deferred", log::Level::Warn,
                   {{"version", version}, {"bad", bad ? "true" : "false"}});
    }
}

void Controller::handlePendingManifest() {
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        if (pendingChanged_) {
            current_ = pending_;
            pendingChanged_ = false;
            nextAttemptAtMs_ = 0;
        }
    }
    if (!current_ || uptimeMs() < nextAttemptAtMs_) return;
    const Manifest m = *current_;

    ManifestDecision decision = decideManifest(record_, m, runningVersion_, runningPendingVerify_, kMaxAttempts);
    if (decision == ManifestDecision::IgnoreBusy) {
        // Retry once the current validation or probe has finished.
        nextAttemptAtMs_ = uptimeMs() + kGateRetryMs;
        return;
    }
    if (decision != ManifestDecision::Install) {
        const char* why = decision == ManifestDecision::IgnoreSameVersion ? "same_version"
                          : decision == ManifestDecision::IgnoreBad       ? "bad_release"
                                                                          : "attempts_exhausted";
        ESP_LOGI(TAG, "update to %s not installed: %s", m.version.c_str(), why);
        if (decision != ManifestDecision::IgnoreSameVersion) {
            log::event("ota_request_ignored", log::Level::Info,
                       {{"version", m.version}, {"requestId", m.requestId}, {"reason", why}});
        }
        current_.reset();
        return;
    }
    if (remote_.transport != RemoteTransport::Wifi) {
        ESP_LOGW(TAG, "updates are only supported over WiFi");
        log::event("ota_failed", log::Level::Warn, {{"version", m.version}, {"reason", "transport"}});
        current_.reset();
        return;
    }
    if (g_gate && !g_gate()) {
        ESP_LOGI(TAG, "update to %s postponed by the project", m.version.c_str());
        nextAttemptAtMs_ = uptimeMs() + kGateRetryMs;
        return;
    }

    log::event("ota_download_started", log::Level::Info,
               {{"version", m.version}, {"from", runningVersion_}, {"requestId", m.requestId}});
    Result result = download(m);
    if (!result.success) {
        ESP_LOGW(TAG, "update to %s failed: %s", m.version.c_str(), result.message.c_str());
        log::event("ota_failed", log::Level::Warn,
                   {{"version", m.version}, {"requestId", m.requestId}, {"reason", result.message}});
        // A transfer that broke halfway is worth retrying; an image that
        // arrived whole but is wrong (hash, size, header, version) will be
        // just as wrong next time.
        if (result.message.rfind("download", 0) == 0) {
            nextAttemptAtMs_ = uptimeMs() + kDownloadRetryMs;
        } else {
            markBad(record_, m.version, m.requestId);
            save();
            current_.reset();
        }
        return;
    }

    recordInstalled(record_, m, runningVersion_);
    save();
    log::event("ota_installed", log::Level::Info,
               {{"version", m.version}, {"from", runningVersion_}, {"attempt", std::to_string(record_.attempts)}});
    ESP_LOGI(TAG, "update to %s written, restarting", m.version.c_str());
    vTaskDelay(pdMS_TO_TICKS(3000));  // let the event reach the journal and, if possible, the server
    while (g_gate && !g_gate()) vTaskDelay(pdMS_TO_TICKS(kGateRetryMs));
    restart();
}

void Controller::restart() {
    if (g_beforeRestart) g_beforeRestart();
    esp_restart();
}

Result Controller::download(const Manifest& m) {
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!target) return Result(false, "no_partition");
    if (m.size > target->size) return Result(false, "size");

    UserCredentials creds = Storage::instance().readUserCredentials();
    const NetworkManager& net = NetworkManager::instance();

    esp_http_client_config_t cfg = {};
    cfg.host = net.serverAddress.c_str();
    cfg.port = m.port != 0 ? m.port : net.serverPort;
    cfg.path = m.path.c_str();
    cfg.transport_type = HTTP_TRANSPORT_OVER_TCP;
    cfg.timeout_ms = kHttpTimeoutMs;
    cfg.buffer_size = 2048;
    cfg.keep_alive_enable = true;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return Result(false, "download: client");
    esp_http_client_set_header(client, "X-Device-Uuid", MessageBus::instance().getDeviceUuid().c_str());
    esp_http_client_set_header(client, "X-Device-Token", creds.deviceToken.c_str());

    std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[kChunkSize]);
    if (!buf) {
        esp_http_client_cleanup(client);
        return Result(false, "download: no memory");
    }

    auto fail = [&](esp_ota_handle_t ota, const std::string& why) {
        if (ota) esp_ota_abort(ota);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return Result(false, why);
    };

    ESP_LOGI(TAG, "downloading %s from %s:%d%s into %s", m.version.c_str(), cfg.host, cfg.port, cfg.path,
             target->label);
    if (esp_http_client_open(client, 0) != ESP_OK) return fail(0, "download: connect");
    int64_t contentLength = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) return fail(0, "download: http " + std::to_string(status));
    if (contentLength >= 0 && contentLength != static_cast<int64_t>(m.size)) return fail(0, "size");

    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) return fail(0, "no_partition");

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, /*is224=*/0);

    uint32_t total = 0;
    uint64_t startedMs = uptimeMs();
    uint32_t nextProgress = 256 * 1024;
    for (;;) {
        int n = esp_http_client_read(client, reinterpret_cast<char*>(buf.get()), kChunkSize);
        if (n < 0) {
            mbedtls_sha256_free(&sha);
            return fail(ota, "download: read");
        }
        if (n == 0) {
            if (esp_http_client_is_complete_data_received(client)) break;
            mbedtls_sha256_free(&sha);
            return fail(ota, "download: truncated");
        }
        if (total + static_cast<uint32_t>(n) > m.size) {
            mbedtls_sha256_free(&sha);
            return fail(ota, "size");
        }
        if (esp_ota_write(ota, buf.get(), n) != ESP_OK) {
            mbedtls_sha256_free(&sha);
            return fail(ota, "flash_write");
        }
        mbedtls_sha256_update(&sha, buf.get(), n);
        total += n;
        if (total >= nextProgress) {
            ESP_LOGI(TAG, "%" PRIu32 " / %" PRIu32 " KB", total / 1024, m.size / 1024);
            nextProgress += 256 * 1024;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (total != m.size) {
        esp_ota_abort(ota);
        return Result(false, "size");
    }
    char hex[65];
    for (int i = 0; i < 32; ++i) std::snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    if (m.sha256 != hex) {
        esp_ota_abort(ota);
        return Result(false, "sha256");
    }
    // Checks the image header, segments and appended SHA-256 (and the
    // signature, once signed images are enabled).
    if (esp_ota_end(ota) != ESP_OK) return Result(false, "image");

    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(target, &desc) != ESP_OK ||
        std::strncmp(desc.version, m.version.c_str(), sizeof(desc.version)) != 0) {
        return Result(false, "version");
    }
    if (esp_ota_set_boot_partition(target) != ESP_OK) return Result(false, "set_boot");

    ESP_LOGI(TAG, "downloaded %" PRIu32 " KB in %" PRIu64 " s", total / 1024, (uptimeMs() - startedMs) / 1000);
    return Result(true, "ok");
}

}  // namespace

void registerSelfTest(const char* name, std::function<bool()> check) {
    g_selfTests.push_back(SelfTest{name, std::move(check)});
}

void setUpdateGate(std::function<bool()> canUpdateNow) { g_gate = std::move(canUpdateNow); }

void setBeforeRestart(std::function<void()> prepare) { g_beforeRestart = std::move(prepare); }

void begin(const CroniotConfig::RemoteCfg& remote) {
#if CONFIG_CRONIOT_OTA_ENABLE
    g_controller.begin(remote);
#else
    (void)remote;
#endif
}

void onServerAuthenticated() {
#if CONFIG_CRONIOT_OTA_ENABLE
    g_controller.onServerAuthenticated();
#endif
}

void listen() {
#if CONFIG_CRONIOT_OTA_ENABLE
    g_controller.listen();
#endif
}

}  // namespace croniot::ota
