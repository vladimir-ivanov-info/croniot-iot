#include "AuthenticationController.h"

void AuthenticationController::init(){

  bool forceRegisterDevice = false; //for debugging purposes

    UserCredentials credentials = Storage::instance().readUserCredentials();

   Serial.println("Credentials: [accountEmail=" + credentials.accountEmail + "] " + credentials.accountPassword + " " + credentials.deviceUuid + " " + credentials.deviceToken);

  if(!forceRegisterDevice 
    && credentials.accountEmail != "null" && /*credentials.accountEmail.isNull() &&*/ !credentials.accountEmail.isEmpty() 
   &&  /*credentials.accountPassword != NULL &&*/ !credentials.accountPassword.isEmpty() 
   &&  /*credentials.deviceUuid != NULL &&*/ !credentials.deviceUuid.isEmpty() 
   &&  /*credentials.deviceToken != NULL && */!credentials.deviceToken.isEmpty()){
      Serial.println("Credentials found, trying to log in...");
      Result loginResult = login(credentials);

      Serial.println("MESSSAGE:[" + loginResult.message + "]");

      while(!loginResult.success && loginResult.message.equals("-1")){
        loginResult = login(credentials);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
      }

      if(!loginResult.success && !loginResult.message.equals("-1")){ //TODO only try to register if server returns: "uuid doesn't exist". If it already exists but login failed because wrong credentials, we don't want to register another same IoT in the server
        Serial.println("Login failed, trying to register IoT device...");
        registerDevice();
        registerSensorTypes();
        registerTasks();
      }

  } else {
    Serial.println("Credentials not found.");
    registerDevice();
    registerSensorTypes();
    registerTasks();
  }
  MQTTManager::instance().init();
  SensorsController::instance().init();
}

Result AuthenticationController::login(UserCredentials credentials){
    MessageLogin messageLogin(credentials.accountEmail, credentials.accountPassword, credentials.deviceUuid, credentials.deviceToken);

    String jsonPayload = messageLogin.toJson();

    Result result = sendHttpPost(jsonPayload, ROUTE_IOT_LOGIN);

    Serial.println("Result login:");
    Serial.println(result.toString());

    return result;
}

void AuthenticationController::registerDevice(){
    UserCredentials credentials = Storage::instance().readUserCredentials();

    String accountEmail = credentials.accountEmail;
    String accountUuid = credentials.accountUuid;
    String accountPassword = credentials.accountPassword;
    String deviceUuid = credentials.deviceUuid;
    String deviceName = credentials.deviceName;
    String deviceDescription = credentials.deviceDescription;

    MessageRegisterDevice messageRegisterDevice(accountEmail, accountPassword, deviceUuid, deviceName, deviceDescription);

    String jsonPayload = messageRegisterDevice.toJson();

    String serverAddress = NetworkManager::instance().serverAddress;
    String serverPort = String(NetworkManager::instance().serverPort);

    String serverRegisterCLientUrl = "http://" + serverAddress +  + ":" + serverPort + ROUTE_REGISTER_CLIENT;
    Serial.println("Register device query to: " + serverRegisterCLientUrl);
    
    HTTPClient http;
    http.begin(serverRegisterCLientUrl);

    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonPayload);
    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String response = http.getString();
        http.end();
        Serial.println("Register client response:");
        Serial.println(response);

        if(!response.isEmpty()){
          Result result = parseResult(response);

          if(result.success){
            UserCredentials userCredentials(accountEmail, accountUuid, accountPassword, deviceUuid, result.message, deviceName, deviceDescription);

            Serial.println("Saving new credentials");
            Storage::instance().saveUserCredentials(userCredentials);
            Serial.println("Credentials saved");

            vTaskDelay(3000 / portTICK_PERIOD_MS); //Needed delay because after this saving we read the credentials again and it seems like the saving is asynchronous, while our code here is synchronous.
          } else {
            
          }
        } else {
          Serial.println("Register response body is empty");
        }
        //TODO parse token
    } else {
        Serial.print("HTTP Request failed. Is the server online? Error code: ");
        Serial.println(httpResponseCode);
    }
}

Result AuthenticationController::sendHttpPost(String content, String route){

  Result result(false, "Default result");

    HTTPClient http;
    http.setConnectTimeout(10000); //10 seconds to connect
    http.setTimeout(10000);

    String serverAddress = NetworkManager::instance().serverAddress;
    String serverPort = String(NetworkManager::instance().serverPort);

    String serverRegisterCLientUrl = "http://" + serverAddress + ":" + serverPort + route;
    Serial.println(serverRegisterCLientUrl);
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
        //uninit();
        Serial.print("HTTP Request failed. Error code: "); Serial.println(httpResponseCode);
        result = Result(false, String(httpResponseCode));
        //init();
    }

    http.end();

    return result;
}

void AuthenticationController::uninit(){
  //MQTTManager::instance().uninit();
 //TODO do this inside of MqttManager's uninit SensorsController::instance().uninit();
}

void AuthenticationController::registerSensor(){
    
}

void AuthenticationController::registerSensorTypes(){

  UserCredentials credentials = Storage::instance().readUserCredentials();

  list<SensorType*> sensorTypes = SensorsController::instance().getAllSensorTypes();

  Serial.print("SENSOR TYPES:");
  Serial.println(sensorTypes.size());

  for(auto *sensorType : sensorTypes){
    MessageRegisterSensorType messageRegisterSensor2(credentials.deviceUuid, credentials.deviceToken, *sensorType);

    String json2 = "";
    JsonDocument doc2;
    messageRegisterSensor2.toJson(doc2.to<JsonObject>());
    serializeJsonPretty(doc2, json2);

    Result result2 = sendHttpPost(json2, ROUTE_REGISTER_SENSOR_TYPE);
    Serial.println("result2 Response:");
    Serial.println(result2.toString());
  }
}

void AuthenticationController::registerTasks(){

  UserCredentials credentials = Storage::instance().readUserCredentials();

  list<TaskType> taskTypes = TaskController::instance().getAllTaskTypes();

  for(auto taskType : taskTypes){
    MessageRegisterTaskType messageRegisteTaskType1(credentials.deviceUuid, credentials.deviceToken, taskType);

    String json4 = "";
    JsonDocument doc4;
    messageRegisteTaskType1.toJson(doc4.to<JsonObject>());
    serializeJsonPretty(doc4, json4);

    Serial.println("Registering task:");
    Serial.println(json4);

    Result result4 = sendHttpPost(json4, ROUTE_REGISTER_TASK_TYPE);
    Serial.println("Response:");
    Serial.println(result4.toString());
  }
}

Result AuthenticationController::parseResult(const String& jsonString) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    // Return a default Result object with success as false and empty message
    return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
  }

  bool success = doc["success"];
  String message = doc["message"];

  return Result(success, message);
}