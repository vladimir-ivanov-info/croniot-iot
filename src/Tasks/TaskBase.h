#ifndef TASKBASE_H
#define TASKBASE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <ArduinoJson.h>
#include "HttpController.h"
#include <map>

class TaskBase {

    public:

        struct TaskData {
            int taskUid;
            std::map<int, String> parametersValues;
        };

        virtual ~TaskBase();
        TaskBase(const char* name, uint32_t stackSize, UBaseType_t priority, UBaseType_t core);
        //virtual ~TaskBase() {}
        virtual void handleCallback(const String& topic, byte* payload, unsigned int length) = 0;
        void enqueueMessage(const String& topic, byte* payload, unsigned int length);

        virtual void loop() = 0;
        
        virtual void run() = 0;

        void init(){
            //xTaskCreatePinnedToCore(taskFunction, "name", 4096, this, 1, &taskHandle, 1);
        }



    protected:
        //TaskBase();
        //static void taskFunction(void* pvParameters);

        QueueHandle_t messageQueue;

        struct Message {
            char* topic;
            byte* payload;
            unsigned int length;
        };

        virtual void executeTask(TaskData taskData) = 0;


        String byteArrayToString(byte* data, unsigned int length) {
            // Create a temporary char array to hold the data
            char* temp = new char[length + 1]; // +1 for the null terminator
            memcpy(temp, data, length);
            temp[length] = '\0'; // Null terminate the string

            // Create a String from the char array
            String result = String(temp);

            // Clean up the temporary char array
            delete[] temp;

            return result;
        };

        TaskData processMessage(String message);

    private:
        static void taskFunction(void* pvParameters);
        TaskHandle_t taskHandle;

};

#endif // TASKBASE_H
