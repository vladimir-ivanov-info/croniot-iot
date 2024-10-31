#include "Storage.h"



//TODO refactor, make the load/save methods generic

ServerData Storage::getServerData() {
  ServerData serverData;

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
  }

  if (!LittleFS.exists("/server_data.json")) {
    Serial.println("/server_data.json file does not exist, creating a new one");
    File newFile = LittleFS.open("/server_data.json", "w");
  }

  File file = LittleFS.open("/server_data.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return serverData;
  }

  JsonDocument doc; // Adjust the size based on your JSON structure
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println("Failed to parse JSON from /server_data.json file");
  } else {
    serverData.serverAddress = doc["serverAddress"].as<String>();
  }

  file.close();

  return serverData;
}

void Storage::saveServerData(const ServerData& serverData) {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  File file = LittleFS.open("/server_data.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  JsonDocument doc;

  doc["serverAddress"] = serverData.serverAddress;

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write JSON to file");
  }

  file.close();

  Serial.println("Credentials saved to file");
}

UserCredentials Storage::readUserCredentials() {
  UserCredentials credentials;

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return credentials;
  }

  if (!LittleFS.exists("/credentials.json")) {
    Serial.println("/credentials.json file does not exist, creating a new one");
    File newFile = LittleFS.open("/credentials.json", "w");
  }

  File file = LittleFS.open("/credentials.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return credentials;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println("Failed to parse JSON from file");
  } else {
    credentials.accountEmail = doc["accountEmail"].as<String>();
    credentials.accountUuid = doc["accountUuid"].as<String>();
    credentials.accountPassword = doc["accountPassword"].as<String>();
    credentials.deviceUuid = doc["deviceUuid"].as<String>();
    credentials.deviceName = doc["deviceName"].as<String>();
    credentials.deviceToken = doc["deviceToken"].as<String>();
    credentials.deviceDescription = doc["deviceDescription"].as<String>();
  }

  file.close();

  return credentials;
}

void Storage::saveUserCredentials(const UserCredentials& credentials) {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  File file = LittleFS.open("/credentials.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  JsonDocument doc;

  doc["accountEmail"] = credentials.accountEmail;
  doc["accountUuid"] = credentials.accountUuid;
  doc["accountPassword"] = credentials.accountPassword;
  doc["deviceUuid"] = credentials.deviceUuid;
  doc["deviceName"] = credentials.deviceName;
  doc["deviceToken"] = credentials.deviceToken;
  doc["deviceDescription"] = credentials.deviceDescription;

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write JSON to file");
  }

  file.close();

  Serial.println("Credentials saved to file");
}

void Storage::saveFutureTask(int taskTypeId, TaskData taskData) {
    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return;  // Ensure we stop further execution if mounting fails
    }

    String time = taskData.parametersValues[2];
    int colonPos = time.indexOf(':');
    String hourStr = time.substring(0, colonPos);
    String minuteStr = time.substring(colonPos + 1);
    String fullPath = "/tasks/" + String(taskTypeId) + "/" + hourStr + "/" + minuteStr;

    int elementsCount = 0;
    String* splitArray = StringUtil::split(fullPath, "/", elementsCount);
    //Serial.print("Elements count: "); Serial.println(elementsCount);

    for (int i = 0; i < elementsCount + 1; i++) {
        String currentPath = "/";
        for (int n = 0; n < i; n++) {
            currentPath += splitArray[n];
            if (n != i - 1) {
                currentPath += "/";
            }
        }
        //Serial.print("Creating if not exists:"); Serial.println(currentPath);
        createDirectoryIfNotExists(currentPath);
    }

    //String filePath = fullPath + "/future_task.json";
    String filePath = fullPath + "/task_" + taskData.taskUid;
    //Serial.println("Trying to create file: " + filePath);

    File file = LittleFS.open(filePath, FILE_WRITE);  // Open the file for writing
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    } else {
        // Write data to the file
        String result = "";

        for (const auto& pair : taskData.parametersValues) {
            result += String(pair.first) + "=" + pair.second + "\n";
        }

        // Write the result to the file
        int writtenBytes = file.print(result);
        //Serial.print("Added bytes: "); Serial.println(writtenBytes);
        //Serial.print("Added to file: ");
        Serial.println(result);
        
        file.flush();  // Ensure data is written to the file system
        file.close();  // Close the file to update size
        
        Serial.println("File created and data written successfully.");
    }

    // Reopen the file in read mode to check the size and contents
    file = LittleFS.open(filePath, FILE_READ);
    if (file) {
        Serial.print("Size of the file: "); Serial.println(file.size());
        String fileContent = file.readString();  // Read the whole content of the file
        Serial.println("Contents of the file: ["); Serial.print(fileContent); Serial.println("]");
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }
}

bool Storage::createDirectories(const String& path) {
    String currentPath = "";
    
    // Iterate through the path and create directories as needed
    for (int i = 0; i < path.length(); i++) {
        currentPath += path[i];

        // Check if we've reached a directory level (denoted by '/')
        if (path[i] == '/') {
            if (!LittleFS.exists(currentPath)) {
                Serial.println("Creating directory: " + currentPath);
                if (!LittleFS.mkdir(currentPath)) {
                    Serial.println("Failed to create directory: " + currentPath);
                    return false;
                }
            }
        }
    }

    // Handle cases where the path doesn't end with a '/'
    if (!LittleFS.exists(currentPath)) {
        Serial.println("Creating final directory: " + currentPath);
        if (!LittleFS.mkdir(currentPath)) {
            Serial.println("Failed to create final directory: " + currentPath);
            return false;
        }
    }

    return true;
}


void Storage::createDir(String path){
  Serial.printf("Creating Dir: %s\n", path);
  if(LittleFS.mkdir(path.c_str())){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void Storage::prueba(){

    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return;  // Ensure we stop further execution if mounting fails
    }

  Serial.printf("Creating Dir: /tasks");

  String tasksFolder = "/tasks";
  createDirectoryIfNotExists(tasksFolder);
}

void Storage::createDirectoryIfNotExists(String directoryPath){
  if(!LittleFS.exists(directoryPath)){
    if(LittleFS.mkdir(directoryPath)){
      //Serial.println("/tasks dir created");
    } else {
      Serial.println("/tasks mkdir failed");
    }
  } else {
    //Serial.print(directoryPath); Serial.println(" already exists, skipping.");
  }
}

std::map<int, String> Storage::getTasksForCurrentDateTime(){

  std::map<int, String> result;

  DateTime *currentDateTime = CurrentDateTimeController::instance().getDateTime(); //TODO move this responsibility to TaskController, Storage doesn't need to access time class

  String pathToSeek = "/tasks/2/" + String(currentDateTime->hour) + "/" + currentDateTime->minute + "/";

  std::vector<String> files = getFilesInDirectory(pathToSeek);

  //Serial.println("Files in directory: " + files.size());

  for(String filePath : files){
    //Serial.println(filePath);

    if(LittleFS.exists(filePath)){
      File file = LittleFS.open(filePath, FILE_READ);
      if (file) {
          //Serial.print("Size of the file: "); Serial.println(file.size());
          String fileContent = file.readString();  // Read the whole content of the file
          //result.push_back(fileContent);
          result[2] = fileContent;  //TODO make generic, not only for 2 = def of TASK_LED_TEST
          //Serial.println("Contents of the file: ["); Serial.print(fileContent); Serial.println("]");
          
          //LittleFS.remove(pathToSeek); //[  4828][E][vfs_api.cpp:182] remove(): /tasks/2/17/28/ does not exists or is directory
          String pathToRemove = pathToSeek + file.name(); //    String filePath = fullPath + "/task_" + taskData.taskUid;
          Serial.println("Removing: " + pathToRemove);
          file.close();
          LittleFS.remove(pathToRemove);
          // LittleFS.rmdir(filePath);
      } 
    }
  }
  return result;
}

std::vector<String> Storage::getFilesInDirectory(String directoryPath) {
  std::vector<String> fileList;

  File root = LittleFS.open(directoryPath, "r");
  if (!root) {
    //Serial.println("Failed to open directory");
    return fileList;
  }

  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return fileList;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      // Add the file name to the list
      fileList.push_back(String(file.path()));
    }
    file = root.openNextFile();
  }

  return fileList;
}