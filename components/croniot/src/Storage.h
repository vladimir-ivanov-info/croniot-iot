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
