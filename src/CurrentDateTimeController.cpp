#include "CurrentDateTimeController.h"

void CurrentDateTimeController::taskFunction(void* pvParameters) {
    CurrentDateTimeController* taskInstance = static_cast<CurrentDateTimeController*>(pvParameters);

    const int resyncPeriodicityInSeconds = 60*10;

    int secondsUntilNextSync = resyncPeriodicityInSeconds; //sync time every 10 minutes.
    while (true) {
        vTaskDelay(1*1000 / portTICK_PERIOD_MS);

        secondsUntilNextSync--;

        if(secondsUntilNextSync == 0){
            taskInstance->synchronizeWithServer();
            Serial.println("Current time re-synced");
            secondsUntilNextSync = resyncPeriodicityInSeconds;
        }

        int second = taskInstance->dateTime->second;
        int minute = taskInstance->dateTime->minute;
        int hour = taskInstance->dateTime->hour;

        taskInstance->dateTime->second = (taskInstance->dateTime->second + 1)%60;

        if(taskInstance->dateTime->second == 0){
            taskInstance->dateTime->minute = (taskInstance->dateTime->minute + 1)%60;
            if(taskInstance->dateTime->minute == 0){
                taskInstance->dateTime->hour = (taskInstance->dateTime->hour + 1)%24;
            }
        }
       // Serial.print("Updated time: "); Serial.print(taskInstance->dateTime->hour); Serial.print(":"); Serial.print(taskInstance->dateTime->minute); Serial.print(":");  Serial.print(taskInstance->dateTime->second); Serial.println();
    }
}

bool CurrentDateTimeController::isCurrentTime(String time){
    bool result = false;
    int elements = 0;
    String* timeData = StringUtil::split(time, ":", elements);

    if(elements > 1){
        int hour = timeData[0].toInt();
        int minute = timeData[1].toInt();
        result = dateTime->hour == hour && dateTime->minute == minute;
    }
    return result;
}

void CurrentDateTimeController::synchronizeWithServer(){ //TODO simplify in 1 query later
    Result resultHour = HttpController::instance().sendHttpPost("", "/hour");
    Result resultMinute = HttpController::instance().sendHttpPost("", "/minute");
    Result resultSecond = HttpController::instance().sendHttpPost("", "/second");
    Serial.print("Datetime query result: "); Serial.print(resultHour.message); Serial.print(":"); Serial.println(resultMinute.message);

    int hour = resultHour.message.toInt();
    int minute = resultMinute.message.toInt();
    int second = resultSecond.message.toInt();

    dateTime->hour = hour;
    dateTime->minute = minute;
    dateTime->second = second;
};

void CurrentDateTimeController::run(){
    synchronizeWithServer();
    
    xTaskCreatePinnedToCore(taskFunction, "TaskDateTime", 4096, this, 1, &taskHandle, 1);
}