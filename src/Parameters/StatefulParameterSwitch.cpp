#include "StatefulParameterSwitch.h"

Parameter StatefulParameterSwitch::toParameter(){
    std::map<String, String> constraints;
    constraints.insert({String("state_1"), String("on")});
    constraints.insert({String("state_2"), String("off")});
    
    Parameter parameter(id, name, "stateful", description, "", constraints);

    return parameter;
}