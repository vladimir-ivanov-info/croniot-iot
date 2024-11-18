#ifndef STATEFULPARAMETERSWITCH_H
#define STATEFULPARAMETERSWITCH_H

#include "Tasks/Parameter.h"

class StatefulParameterSwitch {

    public:
        StatefulParameterSwitch(int id, String name, String description) : id(id), name(name), description(description){}
        StatefulParameterSwitch(String name, String description) : name(name), description(description){}

        Parameter toParameter();

    private:
        int id = DEFAULT_ID;
        String name;
        String description;

        static const int DEFAULT_ID = 1;

};

#endif