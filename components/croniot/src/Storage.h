// #ifndef STORAGE_H
// #define STORAGE_H

// #include <LittleFS.h>
// #include <cJSON.h>

// #include "UserCredentials.h"
// #include "Tasks/TaskData.h"

// #include "StringUtil.h"

// #include <list>
// #include "CurrentDateTimeController.h"

// #include <vector>

// struct ServerData {
//     String serverAddress;
//     //uint16_t serverPort;
// };

// class Storage {

//     public:
//         static Storage & instance() {
//             static  Storage * _instance = 0;
//             if ( _instance == 0 ) {
//                 _instance = new Storage();
//             }
//             return *_instance;
//         }

//         ServerData getServerData();
//         void saveServerData(const ServerData& serverData);

//         UserCredentials readUserCredentials();
//         void saveUserCredentials(const UserCredentials& credentials);

//         void saveFutureTask(int taskTypeId, TaskData taskData);
//         void getNextFutureTask();

//         void prueba();

//         std::map<int, String> getTasksForCurrentDateTime();


//     private:
//         bool createDirectories(const String& path);

//         void createDir(String path);
//         void createDirectoryIfNotExists(String directoryPath);

//         std::vector<String> getFilesInDirectory(String directoryPath);

// };

// #endif



// #ifndef STORAGE_H
// #define STORAGE_H

// #include <string>
// #include <map>
// #include <vector>

// #include "UserCredentials.h"
// #include "Tasks/TaskData.h"
// #include "DateTime.h"

// // Estructura para datos del servidor
// struct ServerData {
//     std::string serverAddress;
// };

// class Storage {
// public:
//     static Storage& instance() {
//         static Storage instance;
//         return instance;
//     }

//     ServerData getServerData();
//     void saveServerData(const ServerData& serverData);

//     UserCredentials readUserCredentials();
//     void saveUserCredentials(const UserCredentials& credentials);

//     void saveFutureTask(int taskTypeId, const TaskData& taskData);
//     std::map<int, std::string> getTasksForCurrentDateTime(const DateTime& dateTime);

// private:
//     Storage() = default;
//     ~Storage() = default;

//     void createDirIfNotExists(const std::string& path);
//     std::vector<std::string> getFilesInDirectory(const std::string& path);
// };

// #endif // STORAGE_H




#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <map>
#include <vector>
#include <cstdio>

//#include "esp_123log.h"
//#include "esp_123littlefs.h"   // Usamos LittleFS en lugar de SPIFFS
#include "cJSON.h"

#include "UserCredentials.h"
#include "Tasks/TaskData.h"
#include "CurrentDateTimeController.h"

struct ServerData {
    std::string serverAddress;
};

class Storage {
public:
    static Storage& instance() {
        static Storage* _instance = nullptr;
        if (_instance == nullptr) {
            _instance = new Storage();
        }
        return *_instance;
    }

    ServerData getServerData();
    void saveServerData(const ServerData& serverData);

    UserCredentials readUserCredentials();
    void saveUserCredentials(const UserCredentials& credentials);

    void saveFutureTask(int taskTypeId, TaskData taskData);
    std::map<int, std::string> getTasksForCurrentDateTime();

private:
    Storage();   // Implementado en .cpp (monta LittleFS)
    ~Storage();  // Implementado en .cpp (desmonta LittleFS)

    bool fileExists(const char* path);
    std::string readFile(const char* path);
    bool writeFile(const char* path, const std::string& content);

    std::vector<std::string> getFilesInDirectory(const std::string& directoryPath);
};

#endif // STORAGE_H
