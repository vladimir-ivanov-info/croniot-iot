#ifndef SIM7600HTTPCONTROLLER_H
#define SIM7600HTTPCONTROLLER_H

#include "HttpController.h"
#include "network/sim7600/Sim7600.h"

class Sim7600HttpController : public HttpController {

    public:

        static Sim7600HttpController& instance() {
            static Sim7600HttpController _instance;  // No need to use a pointer
            return _instance;
        }

        Result sendHttpPost(String content, String route) override;

    private:


};

#endif