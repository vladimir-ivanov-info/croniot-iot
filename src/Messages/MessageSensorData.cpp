#include "MessageSensorData.h"

String MessageSensorData::toString(){
    String json = "{\n";
      //json += "  \"messageName\": \"" + messageName + "\",\n";
      json += "  \"sensorTypeId\": " + String(sensorTypeId) + ",\n";
      json += "  \"value\": \"" + value + "\"\n";
      json += "}";
    return json;
}