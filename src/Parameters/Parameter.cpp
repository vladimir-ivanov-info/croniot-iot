#include "Parameter.h"

String Parameter::toJson() {
    JsonDocument doc;
    
    doc["uid"] = uid;
    doc["name"] = name;
    doc["type"] = type;
    doc["description"] = description;
    doc["unit"] = unit;
    JsonObject constraintsJson = doc["constraints"].add<JsonObject>();

    for (const auto& kv : constraints) {
        constraintsJson[kv.first] = kv.second;
    }

    String result;
    serializeJsonPretty(doc, result);
    return result;
}