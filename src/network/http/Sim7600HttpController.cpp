#include "Sim7600HttpController.h"



Result Sim7600HttpController::sendHttpPost(String content, String route){
    return Sim7600::instance().sendHttpPost(content, route);
}
