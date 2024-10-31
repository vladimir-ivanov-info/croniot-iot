#include "HttpController.h"
#include "NetworkManager.h"

Result HttpController::sendHttpPost(String content, String route){
    Result result(false, "Default result");

    HTTPClient http;

    String serverAddress = NetworkManager::instance().serverAddress;
    uint16_t serverPort = NetworkManager::instance().serverPort;

    String serverRegisterCLientUrl = "http://" + serverAddress + ":" + String(serverPort) + route;

    http.begin(serverRegisterCLientUrl);

    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(content);
    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String response = http.getString();
        if(!response.isEmpty()){
          result = parseResult(response);
        } else {
          Serial.println("HTTP response body is empty for endpoint: " + route);
        }
    } else {
        Serial.print("HTTP Request failed. Error code: ");
        Serial.println(httpResponseCode);
        result = Result(false, String(httpResponseCode));
    }

    http.end();

    return result;
}

Result HttpController::parseResult(const String& jsonString) {

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
  }

  bool success = doc["success"];
  String message = doc["message"];

  return Result(success, message);
}