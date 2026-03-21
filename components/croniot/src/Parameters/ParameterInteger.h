#ifndef PARAMETER_INTEGER_H
#define PARAMETER_INTEGER_H

#include "Parameter.h"

class ParameterInteger : public Parameter {
public:
    ParameterInteger(int uid,
                     const std::string& name,
                     const std::string& description,
                     const std::string& unit,
                     int minValue,
                     int maxValue);
};

#endif
