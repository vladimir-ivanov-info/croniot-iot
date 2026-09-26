#include "Context.h"

namespace croniot::log {

namespace {
thread_local std::vector<std::vector<std::pair<std::string, std::string>>> g_stack;
}  // namespace

Context::Context(std::initializer_list<std::pair<std::string, std::string>> fields)
    : fields_(fields.begin(), fields.end()) {
    g_stack.push_back(fields_);
}

Context::~Context() {
    if (!g_stack.empty()) {
        g_stack.pop_back();
    }
}

std::vector<std::pair<std::string, std::string>> Context::currentFields() {
    std::vector<std::pair<std::string, std::string>> result;
    for (auto it = g_stack.rbegin(); it != g_stack.rend(); ++it) {
        result.insert(result.end(), it->begin(), it->end());
    }
    return result;
}

}  // namespace croniot::log
