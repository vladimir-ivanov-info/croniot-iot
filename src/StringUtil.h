#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <Arduino.h>

class StringUtil{

    public:

    static String* split(String data, String separator, int &elementsCount);

    private:
        static const int MAX_SPLIT_ELEMENTS = 20;


};

#endif