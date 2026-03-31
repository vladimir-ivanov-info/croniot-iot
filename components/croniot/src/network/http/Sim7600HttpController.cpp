#include "Sim7600HttpController.h"

Result Sim7600HttpController::sendHttpPost(const std::string& content, const std::string& route) {
    return Sim7600::instance().sendHttpPost(content, route);
}
