/*#ifndef STRINGUTIL_H
#define STRINGUTIL_H


class StringUtil{

    public:

    static String* split(String data, String separator, int &elementsCount);

    private:
        static const int MAX_SPLIT_ELEMENTS = 20;


};

#endif*/

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>
#include <vector>

class StringUtil {
public:
    static std::vector<std::string> split(const std::string& data, const std::string& separator);

private:
    static const int MAX_SPLIT_ELEMENTS = 20;
};

#endif
