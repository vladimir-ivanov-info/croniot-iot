 #include "ParameterTime.h"

 #include "ParameterInteger.h"

ParameterTime::ParameterTime(int uid, String name, String description) 
: Parameter(uid, name, PARAMETERS_TIME, description, "HH:mm", std::map<String, String>()) 
{

}
