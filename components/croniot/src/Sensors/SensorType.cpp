// #include "SensorType.h"

// SensorType::SensorType(String uid, String name, String description, Parameter parameter)
//     : uid(uid), name(name), description(description) {
//     parameters.push_back(parameter);
// }

// JsonObject SensorType::toJson(JsonObject root) {
//     root["uid"] = uid;
//     root["name"] = name;
//     root["description"] = description;
    
//     JsonArray parametersJson = root["parameters"].to<JsonArray>();

//     for (const Parameter& param : parameters) {
//         JsonObject paramJson = parametersJson.add<JsonObject>();

//         paramJson["name"] = param.name;
//         paramJson["type"] = param.type;
//         paramJson["description"] = param.description;
//         paramJson["unit"] = param.unit;

//         JsonObject constraintsJson = paramJson["constraints"].to<JsonObject>();

//         for (const auto& kv : param.constraints) {
//             constraintsJson[kv.first] = kv.second;
//         }
//     }

//     return root;
// }


#include "SensorType.h"

extern "C" {
    #include "cJSON.h"
}


SensorType::SensorType(const std::string& uid, const std::string& name, const std::string& description, const Parameter& parameter)
    : uid(uid), name(name), description(description) {
    parameters.push_back(parameter);
}

SensorType::SensorType(const std::string& uid, const std::string& name, const std::string& description, const std::list<Parameter>& parameters)
    : uid(uid), name(name), description(description), parameters(parameters) {}

cJSON* SensorType::toJson(cJSON* root) const {
    if (!root) root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "uid", uid.c_str());
    cJSON_AddStringToObject(root, "name", name.c_str());
    cJSON_AddStringToObject(root, "description", description.c_str());

    cJSON* parametersJson = cJSON_AddArrayToObject(root, "parameters");

    for (const Parameter& param : parameters) {
        cJSON* paramJson = cJSON_CreateObject();
        cJSON_AddStringToObject(paramJson, "name", param.name.c_str());
        cJSON_AddStringToObject(paramJson, "type", param.type.c_str());
        cJSON_AddStringToObject(paramJson, "description", param.description.c_str());
        cJSON_AddStringToObject(paramJson, "unit", param.unit.c_str());

        cJSON* constraintsJson = cJSON_AddObjectToObject(paramJson, "constraints");

        for (const auto& kv : param.constraints) {
            cJSON_AddStringToObject(constraintsJson, kv.first.c_str(), kv.second.c_str());
        }

        cJSON_AddItemToArray(parametersJson, paramJson);
    }

    return root;
}
