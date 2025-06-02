#include "AuthenticationController.h"


bool AuthenticationController::init(){

  bool authenticationResult = false;

  bool forceRegisterDevice = false; //for debugging purposes

    UserCredentials credentials = Storage::instance().readUserCredentials();

    Serial.println("---------------------------------------------");
      Serial.println(credentials.accountEmail);;
      Serial.println(credentials.accountPassword);;
    Serial.println("---------------------------------------------");

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
      authenticationResult = true;

      if(!loginResult.success && !loginResult.message.equals("-1")){ //TODO only try to register if server returns: "uuid doesn't exist". If it already exists but login failed because wrong credentials, we don't want to register another same IoT in the server
        Serial.println("Login failed, trying to register IoT device...");
        Result resultRegisterDevice = registerDevice();

        if(resultRegisterDevice.success){
          registerSensorTypes();
          registerTasks();
          authenticationResult = true;
        } else {
          Serial.println("Register device no success: " + resultRegisterDevice.message);
        }
      }
  } else {
    Serial.println("Credentials not found.");
    Result resultRegisterDevice = registerDevice();
    if(resultRegisterDevice.success){
      registerSensorTypes();
      registerTasks();
      authenticationResult = true;
    } else {
      Serial.println("Register device no success: " + resultRegisterDevice.message);
    }
  }

  return authenticationResult;
}

Result AuthenticationController::login(UserCredentials credentials){
    MessageLoginRequest messageLogin(credentials.accountEmail, credentials.accountPassword, credentials.deviceUuid, credentials.deviceToken);

    String jsonPayload = messageLogin.toJson();

    Serial.println("---------------------------------------------");
      Serial.println(credentials.accountEmail);;
      Serial.println(credentials.accountPassword);
      Serial.println(jsonPayload);
    Serial.println("---------------------------------------------");

    Result result = HttpProvider::get()->sendHttpPost(jsonPayload, ROUTE_IOT_LOGIN);

    Serial.println("Result login:");
    Serial.println(result.toString());

    return result;
}

Result AuthenticationController::registerDevice(){
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

    Result result = HttpProvider::get()->sendHttpPost(jsonPayload, ROUTE_REGISTER_CLIENT);

    if(result.success){
      UserCredentials userCredentials(accountEmail, accountUuid, accountPassword, deviceUuid, result.message, deviceName, deviceDescription);

      Serial.println("Saving new credentials");
      Storage::instance().saveUserCredentials(userCredentials);
      Serial.println("Credentials saved");

      vTaskDelay(3000 / portTICK_PERIOD_MS); //Needed delay because after this saving we read the credentials again and it seems like the saving is asynchronous, while our code here is synchronous.
    } else {
      Serial.print("HTTP Request failed. Is the server online? Error code: ");
      Serial.println(result.message);
    }

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

    Result result2 = HttpProvider::get()->sendHttpPost(json2, ROUTE_REGISTER_SENSOR_TYPE);

    Serial.println("result2 Response:");
    Serial.println(result2.toString());
  }
}

void AuthenticationController::registerTasks(){

  UserCredentials credentials = Storage::instance().readUserCredentials();

  list<TaskType> taskTypes = TaskController::instance().getAllTaskTypes();

  for(auto taskType : taskTypes){
    MessageRegisterTaskType messageRegisteTaskType1(credentials.deviceUuid, credentials.deviceToken, taskType);

    String json = "";
    JsonDocument doc4;
    messageRegisteTaskType1.toJson(doc4.to<JsonObject>());
    serializeJsonPretty(doc4, json);

    if(DEBUG_REGISTER_TASKS){
        Serial.println("Registering task:");
        Serial.println(json);
    }

    Result result4 = HttpProvider::get()->sendHttpPost(json, ROUTE_REGISTER_TASK_TYPE);

    if(DEBUG_REGISTER_TASKS){
      Serial.println("Response:");
      Serial.println(result4.toString());
    }
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