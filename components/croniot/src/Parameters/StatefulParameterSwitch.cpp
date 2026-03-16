#include "StatefulParameterSwitch.h"

StatefulParameterSwitch::StatefulParameterSwitch(int id, const std::string& name, const std::string& description)
    : id(id), name(name), description(description) {}

StatefulParameterSwitch::StatefulParameterSwitch(const std::string& name, const std::string& description)
    : name(name), description(description) {}

Parameter StatefulParameterSwitch::toParameter() {
    std::map<std::string, std::string> constraints;
    constraints["state_1"] = "on";
    constraints["state_2"] = "off";

    return Parameter(id, name, "stateful", description, "", constraints);
}
