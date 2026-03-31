#include "StringUtil.h"

std::vector<std::string> StringUtil::split(const std::string& data, const std::string& separator) {
    std::vector<std::string> result;

    if (data.empty() || separator.empty()) {
        return result;
    }

    size_t posStart = 0;
    size_t posEnd;

    while ((posEnd = data.find(separator, posStart)) != std::string::npos) {
        if (posEnd != posStart) {
            result.emplace_back(data.substr(posStart, posEnd - posStart));
        }
        posStart = posEnd + separator.length();
    }

    // Añadir el último segmento (si no está vacío)
    if (posStart < data.length()) {
        result.emplace_back(data.substr(posStart));
    }

    return result;
}
