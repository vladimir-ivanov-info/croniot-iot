#pragma once

// Minimal host stub for esp_log.h: no-op logging macros so ESP-IDF-coupled
// headers (e.g. Tasks/TaskBase.h) parse on host g++ without the ESP-IDF
// toolchain. No host test relies on log output.
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
