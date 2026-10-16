#include "SensorsController.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr TickType_t kFlushTickInterval = pdMS_TO_TICKS(1000);
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
}  // namespace

void SensorsController::init(){
    for(Sensor *sensor : sensors){
        sensor->run();
    }
    // 4096, not 3072: appendRaw() can rotate a Data segment, whose
    // saveIndex() fwrite goes deep into LittleFS.
    xTaskCreate(&SensorsController::flushTask, "croniot_sensor_flush", 4096, nullptr, 1, nullptr);
}

void SensorsController::uninit(){
    //TODO
    //sensorWifiSignal->stop();
    //sensorBattery->stop();
}

void SensorsController::flushTask(void*) {
    for (;;) {
        uint64_t now = nowMs();
        for (Sensor* sensor : SensorsController::instance().sensors) {
            sensor->maybeFlush(now);
        }
        vTaskDelay(kFlushTickInterval);
    }
}
