#include "CommonSetup.h"
#include <WiFi.h>

bool CommonSetup::setupImpl(UserCredentials userCredentials, NetworkConnectionControllerBase* networkConnectionController){

  //setup HTTP controller
  UserCredentials credentialsInMemory = Storage::instance().readUserCredentials();

  Serial.print("Credentials in memory: [");
  Serial.print(credentialsInMemory.accountEmail);
  Serial.print(",");
  Serial.print(credentialsInMemory.accountUuid);
  Serial.print(",");
  Serial.print(credentialsInMemory.accountPassword);
  Serial.print(",");
  Serial.print(credentialsInMemory.deviceUuid);
  Serial.print(",");
  Serial.print(credentialsInMemory.deviceName);
  Serial.print(",");
  Serial.print(credentialsInMemory.deviceDescription);
  Serial.println("]");

  Serial.print("Credentials defined by user: [");
  Serial.print(userCredentials.accountEmail);
  Serial.print(",");
  Serial.print(userCredentials.accountUuid);
  Serial.print(",");
  Serial.print(userCredentials.accountPassword);
  Serial.print(",");
  Serial.print(userCredentials.deviceUuid);
  Serial.print(",");
  Serial.print(userCredentials.deviceName);
  Serial.print(",");
  Serial.print(userCredentials.deviceDescription);
  Serial.println("]");


  if(credentialsInMemory.accountEmail != userCredentials.accountEmail 
  || credentialsInMemory.accountUuid != userCredentials.accountUuid
  || credentialsInMemory.accountPassword != userCredentials.accountPassword 
  || credentialsInMemory.deviceUuid != userCredentials.deviceUuid
  || credentialsInMemory.deviceName != userCredentials.deviceName){

    Serial.println("Storing new credentials");

    UserCredentials newCredentials = 
        UserCredentials(userCredentials.accountEmail, userCredentials.accountUuid, userCredentials.accountPassword, userCredentials.deviceUuid, "", userCredentials.deviceName, userCredentials.deviceDescription);
    
    Storage::instance().saveUserCredentials(newCredentials);
    vTaskDelay(500 / portTICK_PERIOD_MS);

   // return true;
  }

  authenticateWithServer(networkConnectionController);

  return false; //TODO
}

void CommonSetup::authenticateWithServer(NetworkConnectionControllerBase* networkConnectionController){
  bool networkConnectionProviderInitialized = NetworkConnectionProvider::init(networkConnectionController);

  if(networkConnectionProviderInitialized){
    bool authenticated = AuthenticationController::instance().init();

    if(authenticated){
      Serial.println("###AUTHENTICATED WITH SERVER");
      bool mqttInitialized = MqttProvider::get()->init();

      if(mqttInitialized){
        SensorsController::instance().init();
      } else {
        Serial.println("Could not initialize MQTT...");
      }
      
    }
  } else {
    //TODO
    Serial.println("Could not initialize network connection provider... Is server on?"); //TODO ping to check if server on
  }
}


