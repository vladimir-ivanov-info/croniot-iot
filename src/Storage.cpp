#include "Storage.h"



//TODO refactor, make the load/save methods generic

ServerData Storage::getServerData() {
  ServerData serverData;

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
  }

  File file = SPIFFS.open("/server_data.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return serverData;
  }

  JsonDocument doc; // Adjust the size based on your JSON structure
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println("Failed to parse JSON from file");
  } else {
    serverData.serverAddress = doc["serverAddress"].as<String>();
  }

  file.close();

  return serverData;
}

void Storage::saveServerData(const ServerData& serverData) {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  File file = SPIFFS.open("/server_data.json", "w");
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

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return credentials;
  }

  File file = SPIFFS.open("/credentials.json", "r");
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
    credentials.accountPassword = doc["accountPassword"].as<String>();
    credentials.deviceUuid = doc["deviceUuid"].as<String>();
    credentials.deviceToken = doc["deviceToken"].as<String>();
  }

  file.close();

  return credentials;
}

void Storage::saveUserCredentials(const UserCredentials& credentials) {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  File file = SPIFFS.open("/credentials.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  JsonDocument doc;

  doc["accountEmail"] = credentials.accountEmail;
  doc["accountPassword"] = credentials.accountPassword;
  doc["deviceUuid"] = credentials.deviceUuid;
  doc["deviceToken"] = credentials.deviceToken;

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write JSON to file");
  }

  file.close();

  Serial.println("Credentials saved to file");
}