#include "Storage.h"

#include "esp_log.h"
#include "CJsonPtr.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>

#include "esp_littlefs.h"

#include <cstring>    // for strcmp

static const char *TAG = "Storage";

Storage::Storage() {
    // Montar LittleFS al crear la instancia
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = NULL,
        .format_if_mount_failed = true,
        .dont_mount = false
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format LittleFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LittleFS mounted successfully");
    }
}

Storage::~Storage() {
    esp_vfs_littlefs_unregister(NULL);
    ESP_LOGI(TAG, "LittleFS unmounted");
}

ServerData Storage::getServerData() {
    ServerData serverData;
    std::ifstream file("/littlefs/server_data.json");
    if (!file.is_open()) {
        ESP_LOGW(TAG, "Could not open server_data.json");
        return serverData;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    CJsonPtr root(cJSON_Parse(buffer.str().c_str()));
    if (root) {
        cJSON *address = cJSON_GetObjectItem(root.get(), "serverAddress");
        if (cJSON_IsString(address)) {
            serverData.serverAddress = address->valuestring;
        }
    }

    return serverData;
}

void Storage::saveServerData(const ServerData &serverData) {
    CJsonPtr root(cJSON_CreateObject());
    cJSON_AddStringToObject(root.get(), "serverAddress", serverData.serverAddress.c_str());

    char *jsonStr = cJSON_Print(root.get());
    std::ofstream file("/littlefs/server_data.json");
    if (file.is_open()) {
        file << jsonStr;
        file.close();
    }
    cJSON_free(jsonStr);
}

UserCredentials Storage::readUserCredentials() {
    UserCredentials credentials;
    std::ifstream file("/littlefs/credentials.json");
    if (!file.is_open()) {
        ESP_LOGW(TAG, "Could not open credentials.json");
        return credentials;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    CJsonPtr root(cJSON_Parse(buffer.str().c_str()));
    if (root) {
        auto extract = [&](const char *key) -> std::string {
            cJSON *item = cJSON_GetObjectItem(root.get(), key);
            return cJSON_IsString(item) ? std::string(item->valuestring) : std::string("");
        };

        credentials.accountEmail = extract("accountEmail");
        credentials.accountUuid = extract("accountUuid");
        credentials.accountPassword = extract("accountPassword");
        credentials.deviceUuid = extract("deviceUuid");
        credentials.deviceName = extract("deviceName");
        credentials.deviceToken = extract("deviceToken");
        credentials.deviceDescription = extract("deviceDescription");
    }

    return credentials;
}

void Storage::saveUserCredentials(const UserCredentials &credentials) {
    CJsonPtr root(cJSON_CreateObject());
    cJSON_AddStringToObject(root.get(), "accountEmail", credentials.accountEmail.c_str());
    cJSON_AddStringToObject(root.get(), "accountUuid", credentials.accountUuid.c_str());
    cJSON_AddStringToObject(root.get(), "accountPassword", credentials.accountPassword.c_str());
    cJSON_AddStringToObject(root.get(), "deviceUuid", credentials.deviceUuid.c_str());
    cJSON_AddStringToObject(root.get(), "deviceName", credentials.deviceName.c_str());
    cJSON_AddStringToObject(root.get(), "deviceToken", credentials.deviceToken.c_str());
    cJSON_AddStringToObject(root.get(), "deviceDescription", credentials.deviceDescription.c_str());

    char *jsonStr = cJSON_Print(root.get());
    std::ofstream file("/littlefs/credentials.json");
    if (file.is_open()) {
        file << jsonStr;
        file.close();
    }
    cJSON_free(jsonStr);
}

std::map<int, std::string> Storage::getTasksForCurrentDateTime() {
    std::map<int, std::string> result;

    DateTime* currentDateTime = CurrentDateTimeController::instance().getDateTime(); // <-- Esto ya está migrado

    char pathToSeek[64];
    snprintf(pathToSeek, sizeof(pathToSeek), "/tasks/2/%02d/%02d/", currentDateTime->hour, currentDateTime->minute);

    std::vector<std::string> files = getFilesInDirectory(pathToSeek);

    for (const std::string& filePath : files) {
        FILE* file = fopen(filePath.c_str(), "r");
        if (file) {
            fseek(file, 0, SEEK_END);
            size_t size = ftell(file);
            rewind(file);

            std::string fileContent(size, '\0');
            fread(&fileContent[0], 1, size, file);
            fclose(file);

            result[2] = fileContent;

            // Remove the file afterward
            remove(filePath.c_str());
        }
    }

    return result;
}



void Storage::saveFutureTask(int taskTypeId, TaskData taskData) {
    const std::string& time = taskData.parametersValues[2]; // asumiendo clave "2" contiene "HH:MM"
    size_t colonPos = time.find(':');
    if (colonPos == std::string::npos) {
        ESP_LOGW(TAG, "Invalid time format");
        return;
    }

    std::string hourStr = time.substr(0, colonPos);
    std::string minuteStr = time.substr(colonPos + 1);

    std::string basePath = "/tasks/" + std::to_string(taskTypeId) + "/" + hourStr + "/" + minuteStr;
    //std::string filePath = basePath + "/task_" + taskData.taskUid;
    std::string filePath = basePath + "/task_" + std::to_string(taskData.taskUid);


    // 🔧 Crear directorios intermedios
    std::string path = "";
    std::istringstream iss(basePath);
    std::string token;
    while (std::getline(iss, token, '/')) {
        if (token.empty()) continue;
        path += "/" + token;

        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            int res = mkdir(path.c_str(), 0775);
            if (res != 0) {
                ESP_LOGW(TAG, "Failed to create directory: %s", path.c_str());
                return;
            }
        }
    }

    // 💾 Guardar archivo
    FILE* file = fopen(filePath.c_str(), "w");
    if (!file) {
        ESP_LOGW(TAG, "Failed to open file for writing: %s", filePath.c_str());
        return;
    }

    for (const auto& pair : taskData.parametersValues) {
        std::string line = std::to_string(pair.first) + "=" + pair.second + "\n";
        fwrite(line.c_str(), 1, line.length(), file);
    }

    fclose(file);
    ESP_LOGI(TAG, "Future task saved at: %s", filePath.c_str());
}

std::vector<std::string> Storage::getFilesInDirectory(const std::string& directoryPath) {
    std::vector<std::string> fileList;
    DIR* dir = opendir(directoryPath.c_str());
    if (!dir) {
        //TODO ESP_LOGE(TAG, "Error opening directory '%s'", directoryPath.c_str());
        return fileList;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = directoryPath + "/" + entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            // Regular file?
            if (S_ISREG(st.st_mode)) {
                fileList.push_back(fullPath);
            }
        } else {
            ESP_LOGW(TAG, "Could not stat '%s'", fullPath.c_str());
        }
    }

    closedir(dir);
    return fileList;
}