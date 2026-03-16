 /*#include "ParameterTime.h"

 #include "ParameterInteger.h"

ParameterTime::ParameterTime(int uid, String name, String description) 
: Parameter(uid, name, PARAMETERS_TIME, description, "HH:mm", std::map<String, String>()) 
{

}

*/

#include "ParameterTime.h"

ParameterTime::ParameterTime(int uid, const std::string& name, const std::string& description)
    : Parameter(uid, name, PARAMETERS_TIME, description, "HH:mm", {})
{
    // No se añaden constraints adicionales.
}
