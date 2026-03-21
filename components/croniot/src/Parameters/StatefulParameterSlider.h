#ifndef STATEFULPARAMETERSLIDER_H
#define STATEFULPARAMETERSLIDER_H

#include "Parameter.h"
#include "esp_log.h"

#include <string>
#include <map>
#include <type_traits>
#include <cstdio>

class StatefulParameterSlider {

public:
    StatefulParameterSlider() {}
    StatefulParameterSlider(int id, const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue);
    StatefulParameterSlider(const std::string& name, const std::string& description, double stepSize, int minValue, int maxValue);

    Parameter toParameter();

    template<typename T>
    std::string toSafeString(T value, int precision = -1);

private:
    static const int DEFAULT_ID = 1;
    int id = DEFAULT_ID;
    std::string name;
    std::string description;

    double stepSize;
    int minValue;
    int maxValue;

};

template<typename T>
std::string StatefulParameterSlider::toSafeString(T value, int precision) {
    char buf[64];
    if constexpr (std::is_floating_point<T>::value) {
        if (precision >= 0) snprintf(buf, sizeof(buf), "%.*f", precision, value);
        else                snprintf(buf, sizeof(buf), "%f", value);
    } else if constexpr (std::is_integral<T>::value) {
        snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    } else {
        buf[0] = '\0';
    }
    return std::string(buf);
}

#endif
