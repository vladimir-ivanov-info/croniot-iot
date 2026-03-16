#ifndef RESULT_H
#define RESULT_H

#include <string>

class Result {
public:
    Result() = default;
    Result(bool success, const std::string& message)
        : success(success), message(message) {}

    bool success;
    std::string message;

    std::string toString() const;

private:
};

#endif
