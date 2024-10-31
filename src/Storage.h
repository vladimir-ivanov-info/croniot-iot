#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "UserCredentials.h"
#include "Tasks/TaskData.h"

#include "StringUtil.h"

#include <list>
#include "CurrentDateTimeController.h"

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

        void saveFutureTask(int taskTypeId, TaskData taskData);
        void getNextFutureTask();

        void prueba();

        std::map<int, String> getTasksForCurrentDateTime();


    private:
        bool createDirectories(const String& path);

        void createDir(String path);
        void createDirectoryIfNotExists(String directoryPath);

        std::vector<String> getFilesInDirectory(String directoryPath);

};

#endif