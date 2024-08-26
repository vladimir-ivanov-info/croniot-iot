#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "UserCredentials.h"

struct ServerData {
    String serverAddress;
    //uint16_t serverPort;
};

class Storage{

    public:
        static Storage & instance() {
            static  Storage * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new Storage();
            }
            return *_instance;
        }

        ServerData getServerData();
        void saveServerData(const ServerData& serverData);

        UserCredentials readUserCredentials();
        void saveUserCredentials(const UserCredentials& credentials);
    private:


};

#endif