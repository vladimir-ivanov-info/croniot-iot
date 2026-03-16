/*#ifndef SIMPLETASKDATA_H
#define SIMPLETASKDATA_H

#include <map>

class SimpleTaskData {

    public:
        int taskUid;
        std::map<int, String> parametersValues;

        SimpleTaskData(const int& taskUid, const std::map<int, String>& parametersValues) : taskUid(taskUid), parametersValues(parametersValues) {}
};

#endif

*/

#ifndef SIMPLETASKDATA_H
#define SIMPLETASKDATA_H

#include <map>
#include <string>

class SimpleTaskData {
public:
    int taskUid;
    std::map<int, std::string> parametersValues;

    SimpleTaskData(const int& taskUid, const std::map<int, std::string>& parametersValues)
        : taskUid(taskUid), parametersValues(parametersValues) {}
};

#endif
