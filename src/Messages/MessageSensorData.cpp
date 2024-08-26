#include "MessageSensorData.h"

String MessageSensorData::toString(){
    String json = "{\n";
      json += "  \"messageName\": \"" + messageName + "\",\n";
      json += "  \"id\": \"" + String(id) + "\",\n";
      json += "  \"value\": \"" + value + "\",\n";
      json += "}";
    return json;
}