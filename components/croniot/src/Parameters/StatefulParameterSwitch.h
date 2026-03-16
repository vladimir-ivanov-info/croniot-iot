#ifndef STATEFULPARAMETERSWITCH_H
#define STATEFULPARAMETERSWITCH_H

#include "Parameter.h"

class StatefulParameterSwitch {

public:
    StatefulParameterSwitch(int id, const std::string& name, const std::string& description);
    StatefulParameterSwitch(const std::string& name, const std::string& description);

    Parameter toParameter();

private:
    static const int DEFAULT_ID = 1;
    int id = DEFAULT_ID;
    std::string name;
    std::string description;
};

#endif
