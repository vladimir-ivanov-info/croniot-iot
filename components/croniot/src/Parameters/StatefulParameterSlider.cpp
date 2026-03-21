#include "StatefulParameterSlider.h"

StatefulParameterSlider::StatefulParameterSlider(int id, const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue)
    : id(id), name(name), description(description), stepSize(stepSize), minValue(minValue), maxValue(maxValue) {}

StatefulParameterSlider::StatefulParameterSlider(const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue)
    : name(name), description(description), stepSize(stepSize), minValue(minValue), maxValue(maxValue) {}


Parameter StatefulParameterSlider::toParameter() {
    std::map<std::string, std::string> constraints;
    constraints["stepSize"] = toSafeString(stepSize);
    constraints["minValue"] = toSafeString(minValue);
    constraints["maxValue"] = toSafeString(maxValue);

    return Parameter(id, name, "stateful", description, "", std::move(constraints));

}
