#ifndef RESULT_H
#define RESULT_H

#include <string>
#include <utility>

enum class ErrorCode {
    None = 0,
    Unknown,
    NetDown,
    Timeout,
    InvalidArgument,
    NotFound,
    ParseError,
    IoError,
};

class [[nodiscard]] Result {
public:
    Result() = default;
    Result(bool success, const std::string& message)
        : success(success), message(message) {}
    Result(bool success, ErrorCode code, const std::string& message)
        : success(success), code(code), message(message) {}

    bool success = true;
    ErrorCode code = ErrorCode::None;
    std::string message;

    std::string toString() const;

    // Prepends `context` (normally __func__) to the failure chain and
    // returns *this, so an error can cross several layers with one log line
    // at the boundary instead of one per layer. No-op on success.
    Result&& with(const char* context) && {
        if (!success) {
            message = std::string(context) + " <- " + message;
        }
        return std::move(*this);
    }

private:
};

// Propagates a Result failure to the caller, tagging it with the current
// function's name via with(). The enclosing function must itself return
// Result. Success falls through without touching `expr`'s Result.
#define CRONIOT_TRY(expr)                                              \
    do {                                                               \
        Result croniot_try_result_ = (expr);                           \
        if (!croniot_try_result_.success) {                            \
            return std::move(croniot_try_result_).with(__func__);      \
        }                                                               \
    } while (0)

#endif
