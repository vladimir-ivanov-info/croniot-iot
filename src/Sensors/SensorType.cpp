#include "SensorType.h"

SensorType::SensorType(String uid, String name, String description, Parameter parameter)
    : uid(uid), name(name), description(description) {
    parameters.push_back(parameter);
}

JsonObject SensorType::toJson(JsonObject root) {
    root["uid"] = uid;
    root["name"] = name;
    root["description"] = description;
    
    JsonArray parametersJson = root["parameters"].to<JsonArray>();

    for (const Parameter& param : parameters) {
        JsonObject paramJson = parametersJson.add<JsonObject>();

        paramJson["name"] = param.name;
        paramJson["type"] = param.type;
        paramJson["description"] = param.description;
        paramJson["unit"] = param.unit;

        JsonObject constraintsJson = paramJson["constraints"].to<JsonObject>();

        for (const auto& kv : param.constraints) {
            constraintsJson[kv.first] = kv.second;
        }
    }

    return root;
}