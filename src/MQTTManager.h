#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include "WiFiClient.h"
#include "Global.h"
#include <PubSubClient.h>

#include "Tasks/TaskBase.h"

#include <ArduinoJson.h>

#include "HttpController.h"

#include <functional>

#include "Storage.h"

using TopicCallback = std::function<void(String topic, byte* payload, unsigned int length)>;

class MQTTManager{

    public:

        volatile bool initialized = false;

        MQTTManager() : client(espClient) {
           //  this->mutex = xSemaphoreCreateMutex();
              mutex = xSemaphoreCreateMutex();
        }

        static MQTTManager & instance() {
            static MQTTManager * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new MQTTManager();
            }
            return *_instance;
        }

        String topic_outcoming = "";
        String topic_incoming = "";

        void init();
        void uninit();

        void publish(String topic, String message);

        PubSubClient *getClient(){return &client;}

        void registerCallback(String topic, TaskBase* taskInstance);

    private:
        SemaphoreHandle_t mutex; 

        WiFiClient espClient;
        PubSubClient client;

        TaskHandle_t taskHandle = NULL;

        // Static function for the RTOS task
        static void taskFunction(void *param) {
            MQTTManager *self = (MQTTManager *)param;

            // Your RTOS task logic goes here
            for (;;) {
                self->getClient()->loop();
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1000ms
            }
        }

        static void callbackTaskFunction(void* pvParameters);

        struct CallbackTaskParams {
            String topic;
            byte* payload;
            unsigned int length;
            std::function<void(String topic, byte* payload, unsigned int length)> callback;
        };

        void callback(char* topic, byte* payload, unsigned int length);
        std::map<String, TopicCallback> topicCallbacks;
        std::map<String, TaskBase*> topicTaskMap;
};

#endif