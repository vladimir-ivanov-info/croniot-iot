#include "BootCounter.h"

#include <cinttypes>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace croniot::log {

namespace {
constexpr const char* TAG = "BootCounter";
constexpr const char* kNamespace = "croniot_log";
constexpr const char* kKey = "boot";

uint32_t g_cached = 0;
bool g_loaded = false;
}  // namespace

uint32_t BootCounter::current() {
    if (g_loaded) return g_cached;
    g_loaded = true;  // set first: a failure below still yields a stable (if wrong) value for the rest of this boot, never a re-attempt mid-run

    // Log::init() runs as the very first line of app_main (see Log.h's
    // init() doc comment), before anything else - including the
    // firmware's own nvs_flash_init() call, which today happens much
    // later (inside WifiNetworkConnectionController::init(), itself
    // called from CommonSetup::setup()). Calling nvs_flash_init() here is
    // the same defensive, idempotent idiom already used there: ESP-IDF
    // tolerates repeat calls once truly initialized, and a partition
    // that needs erasing only needs it done once regardless of who asks.
    esp_err_t initResult = nvs_flash_init();
    if (initResult == ESP_ERR_NVS_NO_FREE_PAGES || initResult == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "could not open NVS namespace '%s'; bootId stays 0 this boot", kNamespace);
        return g_cached;
    }

    uint32_t count = 0;
    nvs_get_u32(handle, kKey, &count);  // ESP_ERR_NVS_NOT_FOUND on first-ever boot -> count stays 0, next line makes it 1
    ++count;
    if (nvs_set_u32(handle, kKey, count) != ESP_OK || nvs_commit(handle) != ESP_OK) {
        ESP_LOGW(TAG, "could not persist boot counter; using %" PRIu32 " for this boot only", count);
    }
    nvs_close(handle);

    g_cached = count;
    return g_cached;
}

}  // namespace croniot::log
