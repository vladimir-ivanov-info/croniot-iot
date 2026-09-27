#ifndef CRONIOT_LOG_CONTEXT_H
#define CRONIOT_LOG_CONTEXT_H

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace croniot::log {

// MDC-style implicit context (Log4j's Mapped Diagnostic Context): fields
// pushed here are attached to every record captured on this task/thread
// for as long as the Context object lives, without repeating them at each
// call site. Backed by thread_local, which ESP-IDF's FreeRTOS integration
// supports per-task like any other C++11 thread_local.
//
// Usage: croniot::log::Context ctx{{"task", "water"}, {"taskUid", uidStr}};
class Context {
public:
    Context(std::initializer_list<std::pair<std::string, std::string>> fields);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // Innermost (most recently pushed) fields first.
    static std::vector<std::pair<std::string, std::string>> currentFields();

private:
    std::vector<std::pair<std::string, std::string>> fields_;
};

}  // namespace croniot::log

#endif
