#ifndef SIM7600_H
#define SIM7600_H

#include "Arduino.h"
#include <HardwareSerial.h>

#include "Result.h"

#include "esp_task_wdt.h"

#define BAUD_RATE 115200

extern String SIM_PIN_EXTERN;

class Sim7600 {

    public:

        Sim7600();

        static Sim7600 & instance() {
            static  Sim7600 * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new Sim7600();
            }
            return *_instance;
        }

        bool init();
        bool init2();
        bool init3();
        String getResponse(uint32_t timeoutMs = 10000);
        String getResponse2();


        Result sendHttpPost2(String content, String route);
        Result sendHttpPost(String content, String route);

        Result mqttPublish(String content, String route);


        bool checkSimState();

        void printWithHiddenCharacters(String string);

        enum AT_CPIN_RESPONSE {
            UNDEFINED,
            AT_CPIN_RESPONSE_SIM_PIN,
            RDY,
            OK,
            ERROR
        // Constant3,
            // More constants
        };


      /*  const int RXPin = 15;  // RX pin for SoftwareSerial
        const int TXPin = 14;  // TX pin for SoftwareSerial

        //static const uint32_t GPSBaud = 38400;
        static const uint32_t GPSBaud = 9600;

        HardwareSerial ss(2);  // Use UART1, which is available on the ESP32*/

        String checkNetworkRegistration();

    private:
        HardwareSerial sim7600;

        int PIN_PWRK = 18;

        volatile bool initialized;

        bool publishMqttMessage(const String& topic, const String& message);

        bool reconnectToMqtt();



        bool waitForMqttConnect(int timeoutMs = 10000) {
            unsigned long start = millis();
            String response = "";
        
            while (millis() - start < timeoutMs) {
                response += getResponse();
                if (response.indexOf("+CMQTTCONNECT: 0,0") != -1) {
                    Serial.println("✅ MQTT conectado correctamente.");
                    return true;
                }
                if (response.indexOf("+CMQTTCONNECT: 0,") != -1) {
                    Serial.println("❌ Error en conexión MQTT.");
                    return false;
                }
                vTaskDelay(200 / portTICK_PERIOD_MS);  // Espera pequeña antes de seguir leyendo
            }
            
            Serial.println("❌ Timeout esperando a MQTT CONNECT");
            return false;
        }

        String extractJson(const String& input) {
            int start = input.indexOf('{');
            int end = input.lastIndexOf('}');
            if (start != -1 && end != -1 && end > start) {
                return input.substring(start, end + 1);
            }
            return "";  // Invalid JSON structure
        }

        Result parseResult(const String& rawResponse) {
            String jsonString = extractJson(rawResponse);

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonString);
          
            if (error) {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error.f_str());
              return Result(false, "Local deserializeJson() failed, JSON:\n" + jsonString);
            }
          
            bool success = doc["success"];
            String message = doc["message"].as<String>();

            /*Serial.println("-----------");
            for (size_t i = 0; i < message.length(); i++) {
                Serial.print((char)message[i]);
            }
            Serial.println("-----------");*/
          
            return Result(success, message);
        }
};

#endif