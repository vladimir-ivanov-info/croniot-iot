#include "ParameterTime.h"

ParameterTime::ParameterTime(int uid, const std::string& name, const std::string& description)
    : Parameter(uid, name, PARAMETERS_TIME, description, "HH:mm", {})
{
    // No se añaden constraints adicionales.
}
