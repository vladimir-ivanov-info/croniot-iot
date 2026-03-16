/*#ifndef PARAMETER_TIME
#define PARAMETER_TIME

#include "Parameter.h"

class ParameterTime : public Parameter {

    public:
        ParameterTime(int uid, String name, String description);

    private:


};

#endif

*/


#ifndef PARAMETER_TIME_H
#define PARAMETER_TIME_H

#include "Parameter.h"

class ParameterTime : public Parameter {
public:
    ParameterTime(int uid, const std::string& name, const std::string& description);
};

#endif
