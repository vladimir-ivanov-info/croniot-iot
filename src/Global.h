#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include "EEPROM.h"

class Global{

    public:
        static Global & instance() {
            static  Global * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new Global();
            }
            return *_instance;
        }

        void EEPROMsaveUUID(String uuid);
        String EEPROMgetUUID();

        void EEPROMsavePassword(String password);
        String EEPROMgetPassword();

        void EEPROMsaveSessionId(String sessionId);
        String EEPROMgetSessionId();

        String generate9DigitRandomNumber();

        void registerDevice();

        void registerTasks();

        void setClock();
        String getCurrentDateTime();

        byte getCurrentSecond();
        byte getCurrentMinute();
        byte getCurrentHour();
        byte getCurrentDay();
        byte getCurrentMonth();
        byte getCurrentYear();

        bool deviceRegistered = true;

        String tempPassword;
        String tempUuid;

        String currentDate = "";

        String voltage = "0.0V";

        String wifiStrength = "-0 dBm";
        String chipTemperature = "0 ºC";

    private:
        Global();

};

#endif
