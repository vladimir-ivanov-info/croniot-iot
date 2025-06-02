#ifndef CURRENTDATETIMECONTROLLER_H
#define CURRENTDATETIMECONTROLLER_H

#include <WiFiUdp.h>

#include "DateTime.h"
#include "network/http/HttpController.h"
#include "network/http/HttpProvider.h"

#include "StringUtil.h"

class CurrentDateTimeController {

    public:

        static CurrentDateTimeController& instance() {
            static CurrentDateTimeController _instance;  // Simplified singleton
            return _instance;
        }

        void synchronizeWithServer(); //TODO simplify in 1 query later

        void run();

        DateTime *getDateTime(){ return dateTime; }

        bool isCurrentTime(String time);

    private:
        CurrentDateTimeController() : dateTime(new DateTime()){}

        DateTime *dateTime;

        static void taskFunction(void* pvParameters);
        TaskHandle_t taskHandle;
};


#endif