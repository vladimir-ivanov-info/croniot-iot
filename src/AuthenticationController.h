#ifndef AUTHENTICATIONCONTROLLER_H
#define AUTHENTICATIONCONTROLLER_H

#include <SPIFFS.h>

#include <HTTPClient.h>

//#include "secrets.h"

#include "Messages/MessageRegisterDevice.h"
#include "Messages/MessageRegisterSensorType.h"
#include "Messages/MessageRegisterTaskType.h"
#include "Messages/MessageLogin.h"

#include "MQTTManager.h"

#include "Sensors/SensorsController.h"
#include "Tasks/TaskController.h"

#include <map>

//#include "Tasks/Heartbeat/SystemTaskHeartbeat.h"
/*
struct UserCredentials {
    String accountEmail;
    String accountPassword;
    String deviceUuid;
    String deviceToken;
};
*/
class AuthenticationController{

    public:
        static AuthenticationController & instance() {
            static AuthenticationController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new AuthenticationController();
            }
            return *_instance;
        }

        void init(); //TODO pass user credentials, sensors and tasks

        void registerDevice();
        void registerSensor();

        void registerSensorTypes();
        
        void registerTasks();

        Result sendHttpPost(String content, String route);
        
        Result login(UserCredentials credentials);

        Result parseResult(const String& jsonString);
    private:

        String ROUTE_REGISTER_TASK_TYPE = "/api/register_task_type";
        String ROUTE_REGISTER_SENSOR_TYPE = "/api/register_sensor_type";

        bool registered = false;
        bool authentified = false;


        void uninit();

};

#endif