#include "StatefulParameterSlider.h"

StatefulParameterSlider::StatefulParameterSlider(int id, const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue)
    : id(id), name(name), description(description), stepSize(stepSize), minValue(minValue), maxValue(maxValue) {}

StatefulParameterSlider::StatefulParameterSlider(const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue)
    : name(name), description(description), stepSize(stepSize), minValue(minValue), maxValue(maxValue) {}



//StatefulParameterSlider::StatefulParameterSlider(const std::string& name, const std::string& description)
//    : name(name), description(description) {}



Parameter StatefulParameterSlider::toParameter() {

    ESP_LOGI("TAGGGGGGGGGGGGGGGGGG", "----------------> ZZZZZZZZZZZZZZZZZZZZZZZZ %f", stepSize);
    //ESP_LOGI("TAGGGGGGGGGGGGGGGGGG", "----------------> ZZZZZZZZZZZZZZZZZZZZZZZZ %s", toSafeString(stepSize).c_str());

    std::map<std::string, std::string> constraints;
    constraints["stepSize"] = toSafeString(stepSize);
    constraints["minValue"] = toSafeString(minValue);
    constraints["maxValue"] = toSafeString(maxValue);

    //constraints["stepSize"] = std::to_string(stepSize);
    //constraints["minValue"] = std::to_string(minValue);
    //constraints["maxValue"] = std::to_string(maxValue);

    ESP_LOGI("TAGGGGGGGGGGGGGGGGGG", "----------------> AAAAAAAAAAAAAAAAAAA");


    //return Parameter(id, name, "stateful", description, "", constraints);
    return Parameter(id, name, "stateful", description, "", std::move(constraints));

}
