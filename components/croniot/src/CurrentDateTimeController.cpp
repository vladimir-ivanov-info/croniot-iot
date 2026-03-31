// CurrentDateTimeController.cpp
#include "CurrentDateTimeController.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "CurrentDateTime";

void CurrentDateTimeController::taskFunction(void* pvParameters) {
    CurrentDateTimeController* self = static_cast<CurrentDateTimeController*>(pvParameters);

    self->synchronizeWithServer();

    const int resyncPeriodicityInSeconds = 600;
    int secondsUntilNextSync = resyncPeriodicityInSeconds;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        secondsUntilNextSync--;

        if (secondsUntilNextSync <= 0) {
            self->synchronizeWithServer();
            ESP_LOGI(TAG, "Time re-synced with server");
            secondsUntilNextSync = resyncPeriodicityInSeconds;
        }

        self->dateTime->second = (self->dateTime->second + 1) % 60;
        if (self->dateTime->second == 0) {
            self->dateTime->minute = (self->dateTime->minute + 1) % 60;
            if (self->dateTime->minute == 0) {
                self->dateTime->hour = (self->dateTime->hour + 1) % 24;
            }
        }
    }
}

bool CurrentDateTimeController::isCurrentTime(const std::string& time) {
    auto tokens = StringUtil::split(time, ":");
    if (tokens.size() >= 2) {
        int hour = std::stoi(tokens[0]);
        int minute = std::stoi(tokens[1]);
        return dateTime->hour == hour && dateTime->minute == minute;
    }
    return false;
}

void CurrentDateTimeController::synchronizeWithServer() {
    Result resultHour = HttpProvider::get()->sendHttpPost("", "/hour");
    Result resultMinute = HttpProvider::get()->sendHttpPost("", "/minute");
    Result resultSecond = HttpProvider::get()->sendHttpPost("", "/second");

    ESP_LOGI(TAG, "Time from server: %s:%s", resultHour.message.c_str(), resultMinute.message.c_str());

    dateTime->hour = atoi(resultHour.message.c_str());
    dateTime->minute = atoi(resultMinute.message.c_str());
    dateTime->second = atoi(resultSecond.message.c_str());
}

void CurrentDateTimeController::run() {
    xTaskCreate(taskFunction, "TaskDateTime", 4096, this, 1, &taskHandle);
}
