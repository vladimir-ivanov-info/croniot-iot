#ifndef HTTPPROVIDER_H
#define HTTPPROVIDER_H

#include <memory>

#include "HttpController.h"

class HttpProvider {
    public:
        static void set(HttpController* controller) {
            instance_ = controller;
        }
    
        static HttpController* get() {
            return instance_;
        }
    
    private:
        static HttpController* instance_;
};

#endif