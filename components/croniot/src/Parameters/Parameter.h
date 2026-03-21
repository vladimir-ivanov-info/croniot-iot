#ifndef PARAMETER_H
#define PARAMETER_H

#include <string>
#include <map>

#define PARAMETERS_NUMBER "number"
#define PARAMETERS_STRING "string"
#define PARAMETERS_TIME "time"

#define PARAMETERS_NUMBER_MIN_VALUE "minValue"
#define PARAMETERS_NUMBER_MAX_VALUE "maxValue"

class Parameter {
public:
  /*  Parameter(int uid,
              const std::string& name,
              const std::string& type,
              const std::string& description,
              const std::string& unit,
              const std::map<std::string, std::string>& constraints);
*/

        Parameter(int uid,
              const std::string name,
              const std::string type,
              const std::string description,
              const std::string unit,
              const std::map<std::string, std::string> constraints);

            /*Parameter(int id,
              std::string name,
              std::string type,
              std::string description,
              std::string unit,
              std::map<std::string, std::string> constraints = {}
            ) : id(id),
                name(std::move(name)),
                type(std::move(type)),
                description(std::move(description)),
                unit(std::move(unit)),
                constraints(std::move(constraints)) {}*/

    std::string toJson() const;

//p123rivate:
    int uid;
    std::string name;
    std::string type;
    std::string description;
    std::string unit;
    std::map<std::string, std::string> constraints;
};

#endif
