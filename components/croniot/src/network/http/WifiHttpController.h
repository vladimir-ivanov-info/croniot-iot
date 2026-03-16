#ifndef WIFIHTTPCONTROLLER_H
#define WIFIHTTPCONTROLLER_H

#include <string>
#include "Result.h"
#include "HttpController.h"

class NetworkManager;  // Forward declaration

class WifiHttpController : public HttpController {

public:
    static WifiHttpController& instance() {
        static WifiHttpController _instance;
        return _instance;
    }

    Result sendHttpPost(const std::string& content, const std::string& route) override;

private:
    // TODO parseResult(const std::string&)
};

#endif
