// #include "Storage.h"



// //TODO refactor, make the load/save methods generic

// ServerData Storage::getServerData() {
//   ServerData serverData;

//   if (!LittleFS.begin(true)) {
//     Serial.println("Failed to mount SPIFFS");
//   }

//   if (!LittleFS.exists("/server_data.json")) {
//     Serial.println("/server_data.json file does not exist, creating a new one");
//     File newFile = LittleFS.open("/server_data.json", "w");
//   }

//   File file = LittleFS.open("/server_data.json", "r");
//   if (!file) {
//     Serial.println("Failed to open file for reading");
//     return serverData;
//   }

//   JsonDocument doc; // Adjust the size based on your JSON structure
//   DeserializationError error = deserializeJson(doc, file);

//   if (error) {
//     Serial.println("Failed to parse JSON from /server_data.json file");
//   } else {
//     serverData.serverAddress = doc["serverAddress"].as<String>();
//   }

//   file.close();

//   return serverData;
// }

// void Storage::saveServerData(const ServerData& serverData) {
//   if (!LittleFS.begin()) {
//     Serial.println("Failed to mount SPIFFS");
//     return;
//   }

//   File file = LittleFS.open("/server_data.json", "w");
//   if (!file) {
//     Serial.println("Failed to open file for writing");
//     return;
//   }

//   JsonDocument doc;

//   doc["serverAddress"] = serverData.serverAddress;

//   if (serializeJson(doc, file) == 0) {
//     Serial.println("Failed to write JSON to file");
//   }

//   file.close();

//   Serial.println("Credentials saved to file");
// }

// UserCredentials Storage::readUserCredentials() {
//   UserCredentials credentials;

//   if (!LittleFS.begin(true)) {
//     Serial.println("Failed to mount SPIFFS");
//     return credentials;
//   }

//   if (!LittleFS.exists("/credentials.json")) {
//     Serial.println("/credentials.json file does not exist, creating a new one");
//     File newFile = LittleFS.open("/credentials.json", "w");
//   }

//   File file = LittleFS.open("/credentials.json", "r");
//   if (!file) {
//     Serial.println("Failed to open file for reading");
//     return credentials;
//   }

//   JsonDocument doc;
//   DeserializationError error = deserializeJson(doc, file);

//   if (error) {
//     Serial.println("Failed to parse JSON from file");
//   } else {
//     credentials.accountEmail = doc["accountEmail"].as<String>();
//     credentials.accountUuid = doc["accountUuid"].as<String>();
//     credentials.accountPassword = doc["accountPassword"].as<String>();
//     credentials.deviceUuid = doc["deviceUuid"].as<String>();
//     credentials.deviceName = doc["deviceName"].as<String>();
//     credentials.deviceToken = doc["deviceToken"].as<String>();
//     credentials.deviceDescription = doc["deviceDescription"].as<String>();
//   }

//   file.close();

//   return credentials;
// }

// void Storage::saveUserCredentials(const UserCredentials& credentials) {
//   if (!LittleFS.begin()) {
//     Serial.println("Failed to mount SPIFFS");
//     return;
//   }

//   File file = LittleFS.open("/credentials.json", "w");
//   if (!file) {
//     Serial.println("Failed to open file for writing");
//     return;
//   }

//   JsonDocument doc;

//   doc["accountEmail"] = credentials.accountEmail;
//   doc["accountUuid"] = credentials.accountUuid;
//   doc["accountPassword"] = credentials.accountPassword;
//   doc["deviceUuid"] = credentials.deviceUuid;
//   doc["deviceName"] = credentials.deviceName;
//   doc["deviceToken"] = credentials.deviceToken;
//   doc["deviceDescription"] = credentials.deviceDescription;

//   if (serializeJson(doc, file) == 0) {
//     Serial.println("Failed to write JSON to file");
//   }

//   file.close();

//   Serial.println("Credentials saved to file");
// }

// void Storage::saveFutureTask(int taskTypeId, TaskData taskData) {
//     // Mount LittleFS
//     if (!LittleFS.begin(true)) {
//         Serial.println("Failed to mount LittleFS");
//         return;  // Ensure we stop further execution if mounting fails
//     }

//     String time = taskData.parametersValues[2];
//     int colonPos = time.indexOf(':');
//     String hourStr = time.substring(0, colonPos);
//     String minuteStr = time.substring(colonPos + 1);
//     String fullPath = "/tasks/" + String(taskTypeId) + "/" + hourStr + "/" + minuteStr;

//     int elementsCount = 0;
//     String* splitArray = StringUtil::split(fullPath, "/", elementsCount);
//     //Serial.print("Elements count: "); Serial.println(elementsCount);

//     for (int i = 0; i < elementsCount + 1; i++) {
//         String currentPath = "/";
//         for (int n = 0; n < i; n++) {
//             currentPath += splitArray[n];
//             if (n != i - 1) {
//                 currentPath += "/";
//             }
//         }
//         //Serial.print("Creating if not exists:"); Serial.println(currentPath);
//         createDirectoryIfNotExists(currentPath);
//     }

//     //String filePath = fullPath + "/future_task.json";
//     String filePath = fullPath + "/task_" + taskData.taskUid;
//     //Serial.println("Trying to create file: " + filePath);

//     File file = LittleFS.open(filePath, FILE_WRITE);  // Open the file for writing
//     if (!file) {
//         Serial.println("Failed to open file for writing");
//         return;
//     } else {
//         // Write data to the file
//         String result = "";

//         for (const auto& pair : taskData.parametersValues) {
//             result += String(pair.first) + "=" + pair.second + "\n";
//         }

//         // Write the result to the file
//         int writtenBytes = file.print(result);
//         //Serial.print("Added bytes: "); Serial.println(writtenBytes);
//         //Serial.print("Added to file: ");
//         Serial.println(result);
        
//         file.flush();  // Ensure data is written to the file system
//         file.close();  // Close the file to update size
        
//         Serial.println("File created and data written successfully.");
//     }

//     // Reopen the file in read mode to check the size and contents
//     file = LittleFS.open(filePath, FILE_READ);
//     if (file) {
//         Serial.print("Size of the file: "); Serial.println(file.size());
//         String fileContent = file.readString();  // Read the whole content of the file
//         Serial.println("Contents of the file: ["); Serial.print(fileContent); Serial.println("]");
//         file.close();
//     } else {
//         Serial.println("Failed to open file for reading");
//     }
// }

// bool Storage::createDirectories(const String& path) {
//     String currentPath = "";
    
//     // Iterate through the path and create directories as needed
//     for (int i = 0; i < path.length(); i++) {
//         currentPath += path[i];

//         // Check if we've reached a directory level (denoted by '/')
//         if (path[i] == '/') {
//             if (!LittleFS.exists(currentPath)) {
//                 Serial.println("Creating directory: " + currentPath);
//                 if (!LittleFS.mkdir(currentPath)) {
//                     Serial.println("Failed to create directory: " + currentPath);
//                     return false;
//                 }
//             }
//         }
//     }

//     // Handle cases where the path doesn't end with a '/'
//     if (!LittleFS.exists(currentPath)) {
//         Serial.println("Creating final directory: " + currentPath);
//         if (!LittleFS.mkdir(currentPath)) {
//             Serial.println("Failed to create final directory: " + currentPath);
//             return false;
//         }
//     }

//     return true;
// }


// void Storage::createDir(String path){
//   Serial.printf("Creating Dir: %s\n", path);
//   if(LittleFS.mkdir(path.c_str())){
//     Serial.println("Dir created");
//   } else {
//     Serial.println("mkdir failed");
//   }
// }

// void Storage::prueba(){

//     if (!LittleFS.begin(true)) {
//         Serial.println("Failed to mount LittleFS");
//         return;  // Ensure we stop further execution if mounting fails
//     }

//   Serial.printf("Creating Dir: /tasks");

//   String tasksFolder = "/tasks";
//   createDirectoryIfNotExists(tasksFolder);
// }

// void Storage::createDirectoryIfNotExists(String directoryPath){
//   if(!LittleFS.exists(directoryPath)){
//     if(LittleFS.mkdir(directoryPath)){
//       //Serial.println("/tasks dir created");
//     } else {
//       Serial.println("/tasks mkdir failed");
//     }
//   } else {
//     //Serial.print(directoryPath); Serial.println(" already exists, skipping.");
//   }
// }

// std::map<int, String> Storage::getTasksForCurrentDateTime(){

//   std::map<int, String> result;

//   DateTime *currentDateTime = CurrentDateTimeController::instance().getDateTime(); //TODO move this responsibility to TaskController, Storage doesn't need to access time class

//   String pathToSeek = "/tasks/2/" + String(currentDateTime->hour) + "/" + currentDateTime->minute + "/";

//   std::vector<String> files = getFilesInDirectory(pathToSeek);

//   //Serial.println("Files in directory: " + files.size());

//   for(String filePath : files){
//     //Serial.println(filePath);

//     if(LittleFS.exists(filePath)){
//       File file = LittleFS.open(filePath, FILE_READ);
//       if (file) {
//           //Serial.print("Size of the file: "); Serial.println(file.size());
//           String fileContent = file.readString();  // Read the whole content of the file
//           //result.push_back(fileContent);
//           result[2] = fileContent;  //TODO make generic, not only for 2 = def of TASK_LED_TEST
//           //Serial.println("Contents of the file: ["); Serial.print(fileContent); Serial.println("]");
          
//           //LittleFS.remove(pathToSeek); //[  4828][E][vfs_api.cpp:182] remove(): /tasks/2/17/28/ does not exists or is directory
//           String pathToRemove = pathToSeek + file.name(); //    String filePath = fullPath + "/task_" + taskData.taskUid;
//           Serial.println("Removing: " + pathToRemove);
//           file.close();
//           LittleFS.remove(pathToRemove);
//           // LittleFS.rmdir(filePath);
//       } 
//     }
//   }
//   return result;
// }

// std::vector<String> Storage::getFilesInDirectory(String directoryPath) {
//   std::vector<String> fileList;

//   File root = LittleFS.open(directoryPath, "r");
//   if (!root) {
//     //Serial.println("Failed to open directory");
//     return fileList;
//   }

//   if (!root.isDirectory()) {
//     Serial.println("Not a directory");
//     return fileList;
//   }

//   File file = root.openNextFile();
//   while (file) {
//     if (!file.isDirectory()) {
//       // Add the file name to the list
//       fileList.push_back(String(file.path()));
//     }
//     file = root.openNextFile();
//   }

//   return fileList;
// }





// #include "Storage.h"

// #include "esp_log.h"
// //#include "esp_vfs_spiffs.h"
// #include "esp_spiffs.h"

// #include "cJSON.h"
// #include <dirent.h>
// #include <sys/stat.h>
// #include <fstream>

// static const char *TAG = "Storage";

// UserCredentials Storage::readUserCredentials() {
//     UserCredentials credentials;

//     FILE* file = fopen("/spiffs/credentials.json", "r");
//     if (!file) {
//         ESP_LOGW(TAG, "/spiffs/credentials.json does not exist, creating...");
//         file = fopen("/spiffs/credentials.json", "w");
//         if (file) fclose(file);
//         return credentials;
//     }

//     fseek(file, 0, SEEK_END);
//     size_t size = ftell(file);
//     rewind(file);

//     char* buffer = (char*)malloc(size + 1);
//     fread(buffer, 1, size, file);
//     buffer[size] = '\0';
//     fclose(file);

//     cJSON* root = cJSON_Parse(buffer);
//     if (!root) {
//         ESP_LOGE(TAG, "Failed to parse credentials.json");
//         free(buffer);
//         return credentials;
//     }

//     credentials.accountEmail = cJSON_GetObjectItem(root, "accountEmail")->valuestring;
//     credentials.accountUuid = cJSON_GetObjectItem(root, "accountUuid")->valuestring;
//     credentials.accountPassword = cJSON_GetObjectItem(root, "accountPassword")->valuestring;
//     credentials.deviceUuid = cJSON_GetObjectItem(root, "deviceUuid")->valuestring;
//     credentials.deviceToken = cJSON_GetObjectItem(root, "deviceToken")->valuestring;
//     credentials.deviceName = cJSON_GetObjectItem(root, "deviceName")->valuestring;
//     credentials.deviceDescription = cJSON_GetObjectItem(root, "deviceDescription")->valuestring;

//     cJSON_Delete(root);
//     free(buffer);

//     ESP_LOGI(TAG, "Credentials loaded");
//     return credentials;
// }

// void Storage::saveUserCredentials(const UserCredentials& credentials) {
//     FILE* file = fopen("/spiffs/credentials.json", "w");
//     if (!file) {
//         ESP_LOGE(TAG, "Failed to open credentials.json for writing");
//         return;
//     }

//     cJSON* root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "accountEmail", credentials.accountEmail.c_str());
//     cJSON_AddStringToObject(root, "accountUuid", credentials.accountUuid.c_str());
//     cJSON_AddStringToObject(root, "accountPassword", credentials.accountPassword.c_str());
//     cJSON_AddStringToObject(root, "deviceUuid", credentials.deviceUuid.c_str());
//     cJSON_AddStringToObject(root, "deviceToken", credentials.deviceToken.c_str());
//     cJSON_AddStringToObject(root, "deviceName", credentials.deviceName.c_str());
//     cJSON_AddStringToObject(root, "deviceDescription", credentials.deviceDescription.c_str());

//     char* jsonString = cJSON_PrintUnformatted(root);
//     fprintf(file, "%s", jsonString);
//     fclose(file);

//     cJSON_Delete(root);
//     free(jsonString);

//     ESP_LOGI(TAG, "Credentials saved");
// }

// ServerData Storage::getServerData() {
//     ServerData serverData;

//     FILE* file = fopen("/spiffs/server_data.json", "r");
//     if (!file) {
//         ESP_LOGW(TAG, "server_data.json not found");
//         return serverData;
//     }

//     fseek(file, 0, SEEK_END);
//     size_t size = ftell(file);
//     rewind(file);

//     char* buffer = (char*)malloc(size + 1);
//     fread(buffer, 1, size, file);
//     buffer[size] = '\0';
//     fclose(file);

//     cJSON* root = cJSON_Parse(buffer);
//     if (root) {
//         serverData.serverAddress = cJSON_GetObjectItem(root, "serverAddress")->valuestring;
//         cJSON_Delete(root);
//     }

//     free(buffer);
//     return serverData;
// }

// void Storage::saveServerData(const ServerData& serverData) {
//     FILE* file = fopen("/spiffs/server_data.json", "w");
//     if (!file) {
//         ESP_LOGE(TAG, "Failed to open server_data.json for writing");
//         return;
//     }

//     cJSON* root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "serverAddress", serverData.serverAddress.c_str());

//     char* jsonString = cJSON_PrintUnformatted(root);
//     fprintf(file, "%s", jsonString);
//     fclose(file);

//     cJSON_Delete(root);
//     free(jsonString);

//     ESP_LOGI(TAG, "Server data saved");
// }





// Storage.cpp
#include "Storage.h"

//#include "esp_littlefs.h"
#include "esp_log.h"
#include "CJsonPtr.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>

//#include "joltwallet__littlefs.h"
#include "esp_littlefs.h"

#include <cstring>    // for strcmp


//#include "vfs_littlefs.h"


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