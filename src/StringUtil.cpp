#include "StringUtil.h"

String* StringUtil::split(String data, String separator, int &elementsCount) {
    // Create a static array to hold the split elements (adjust size as needed)
    static String result[MAX_SPLIT_ELEMENTS];

    int index = 0;
    int previousIndex = 0;
    elementsCount = 0; // Keep track of the number of elements split

    // Ensure there's data to split
    if (data.length() == 0) {
        return result; // Empty array if there's nothing to split
    }

    // Split the string based on the separator
    while ((index = data.indexOf(separator, previousIndex)) != -1 && elementsCount < MAX_SPLIT_ELEMENTS) {
        // Avoid adding empty strings when there are consecutive separators
        if (index > previousIndex) {
            result[elementsCount++] = data.substring(previousIndex, index);
        }
        previousIndex = index + separator.length();
    }

    // Add the last part of the string after the final separator
    if (previousIndex < data.length()) {
        result[elementsCount++] = data.substring(previousIndex);
    }

    return result;
}
