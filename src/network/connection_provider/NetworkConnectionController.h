#ifndef NETWORKCONNECTIONCONTROLLER_H
#define NETWORKCONNECTIONCONTROLLER_H

#include "NetworkConnectionControllerBase.h"


template <typename T>
class NetworkConnectionController : public NetworkConnectionControllerBase {

    public:

        static T& getInstance() {
            static T instance;
            return instance;
        }

        bool init() {
            return static_cast<T*>(this)->init();
        }

        // Remove copy and move
        NetworkConnectionController(const NetworkConnectionController&) = delete;
        NetworkConnectionController& operator=(const NetworkConnectionController&) = delete;
    
        virtual ~NetworkConnectionController() = default;

    protected:
        NetworkConnectionController() = default;


    private:


};

#endif