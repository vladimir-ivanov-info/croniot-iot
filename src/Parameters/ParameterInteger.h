#ifndef PARAMETER_INTEGER
#define PARAMETER_INTEGER

#include "Parameter.h"

class ParameterInteger : public Parameter {

    public:
        ParameterInteger(int uid, String name, /*String type,*/ String description, String unit,/*, std::map<String, String> constraints*/ int minValue, int maxValue);

    private:

};

#endif