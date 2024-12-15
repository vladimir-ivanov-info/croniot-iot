#include "ParameterInteger.h"

ParameterInteger::ParameterInteger(int uid, String name, String description, String unit, int minValue, int maxValue) 
: Parameter(uid, name, PARAMETERS_NUMBER, description, unit, std::map<String, String>()) 
{
    this->constraints.insert({PARAMETERS_NUMBER_MIN_VALUE, String(minValue)});
    this->constraints.insert({PARAMETERS_NUMBER_MAX_VALUE, String(maxValue)});
}