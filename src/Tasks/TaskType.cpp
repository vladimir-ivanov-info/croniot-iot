#include "TaskType.h"

JsonObject TaskType::toJson(JsonObject root){
    root["uid"] = uid;
    root["name"] = name;
    root["description"] = description;

    JsonArray parametersJson = root["parameters"].to<JsonArray>();

    for (const Parameter& param : parameters) {
        JsonObject paramJson = parametersJson.add<JsonObject>();

        paramJson["uid"] = param.uid;
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