#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    // Tu código aquí
       while (true) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
