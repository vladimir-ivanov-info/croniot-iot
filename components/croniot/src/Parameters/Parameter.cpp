/*#include "Parameter.h"

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
}*/


#include "Parameter.h"
#include "cJSON.h"
/*
Parameter::Parameter(int uid,
                     const std::string& name,
                     const std::string& type,
                     const std::string& description,
                     const std::string& unit,
                     const std::map<std::string, std::string>& constraints)
    : uid(uid), name(name), type(type), description(description), unit(unit), constraints(constraints) {}*/

Parameter::Parameter(int uid,
                     const std::string name,
                     const std::string type,
                     const std::string description,
                     const std::string unit,
                     const std::map<std::string, std::string> constraints)
    : uid(uid), name(name), type(type), description(description), unit(unit), constraints(constraints) {}

std::string Parameter::toJson() const {
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "uid", uid);
    cJSON_AddStringToObject(root, "name", name.c_str());
    cJSON_AddStringToObject(root, "type", type.c_str());
    cJSON_AddStringToObject(root, "description", description.c_str());
    cJSON_AddStringToObject(root, "unit", unit.c_str());

    cJSON* constraintsJson = cJSON_CreateObject();
    for (const auto& kv : constraints) {
        cJSON_AddStringToObject(constraintsJson, kv.first.c_str(), kv.second.c_str());
    }
    cJSON_AddItemToObject(root, "constraints", constraintsJson);

    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string result(jsonStr);

    cJSON_Delete(root);
    free(jsonStr);

    return result;
}
