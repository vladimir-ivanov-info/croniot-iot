#ifndef COMMONSETUP_H
#define COMMONSETUP_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "CroniotConfig.h"

class CommonSetup {
public:
    static CommonSetup& instance() {
        static CommonSetup _instance;
        return _instance;
    }

    bool setup(const croniot::CroniotConfig& config);

private:
    static void authenticateWithServerTask(void* pvParameters);
    TaskHandle_t authenticateWithServerTaskTaskHandle = nullptr;
};

#endif
