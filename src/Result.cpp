#include "Result.h"

String Result::toString(){
    JsonDocument doc;

    JsonObject json = doc.to<JsonObject>();
    
    json["success"] = success;
    json["message"] = message;

    String jsonString;
    serializeJsonPretty(doc, jsonString);

    return jsonString;
}