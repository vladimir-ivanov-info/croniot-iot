#ifndef SIM7600_H
#define SIM7600_H

#include <string>
#include <vector>
#include "driver/uart.h"
#include "Result.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SIM7600_UART_NUM      UART_NUM_1
#define SIM7600_UART_TX_PIN   GPIO_NUM_17
#define SIM7600_UART_RX_PIN   GPIO_NUM_16
#define SIM7600_UART_BUF_SIZE 1024
#define SIM7600_BAUD_RATE     115200

extern std::string SIM_PIN_EXTERN;

class Sim7600 {
public:
    static Sim7600& instance();

    bool init();
    std::string getResponse(uint32_t timeoutMs = 10000);
    Result sendHttpPost(const std::string& content, const std::string& route);
    Result mqttPublish(const std::string& topic, const std::string& message);

private:
    Sim7600();
    void uartInit();
    void uartWrite(const std::string& data);
    size_t uartRead(char* buf, size_t len, uint32_t timeoutMs);

    bool initialized = false;
    int  PIN_PWRK = 18;

    enum AT_CPIN_RESPONSE {
        UNDEFINED,
        SIM_PIN,
        RDY,
        OK,
        ERROR
    };

    AT_CPIN_RESPONSE parseCpin(const std::string& resp);
    std::string extractJson(const std::string& input);
    Result parseResult(const std::string& rawResponse);
};

#endif // SIM7600_H
