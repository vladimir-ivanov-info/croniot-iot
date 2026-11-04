#include "WifiSetupConsole.h"

#include <cstdio>
#include <string>

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
#include "driver/usb_serial_jtag.h"
#endif
#if CONFIG_ESP_CONSOLE_UART
#include "driver/uart.h"
#endif

#include "Storage.h"
#include "WifiSetupDialog.h"
#include "log/Log.h"
#include "network/connection_provider/WifiNetworkConnectionController.h"

// Fallbacks for a build whose sdkconfig predates this Kconfig menu.
#ifndef CONFIG_CRONIOT_WIFI_CONSOLE_TASK_STACK_SIZE
#define CONFIG_CRONIOT_WIFI_CONSOLE_TASK_STACK_SIZE 4096
#endif
#ifndef CONFIG_CRONIOT_WIFI_CONNECT_TIMEOUT_S
#define CONFIG_CRONIOT_WIFI_CONNECT_TIMEOUT_S 30
#endif

namespace croniot::provision {

#if CONFIG_CRONIOT_WIFI_CONSOLE

namespace {

const char* TAG = "WifiSetup";

constexpr uint32_t kReminderMs = 60 * 1000;
constexpr TickType_t kReadTicks = pdMS_TO_TICKS(50);

struct State {
    Source source = Source::None;
    WifiCredentials inUse;
    std::function<bool()> startWifi;
    WifiSetupDialog dialog;
    LineEditor editor;
    WifiCredentials beforeAttempt;
    // After a failed attempt the driver is parked (no retries) while the
    // person decides; saving anyway has to start it again.
    bool parked = false;
};

State* g_state = nullptr;

// Straight to stdout, not through ESP_LOG: these lines are for whoever
// is at the serial monitor, not for the journal or the server.
void print(const std::string& text) {
    if (text.empty()) return;
    fputs(text.c_str(), stdout);
    fflush(stdout);
}

void printBanner() {
    print("\r\n"
          "==================================================================\r\n"
          "  WiFi is not configured on this device.\r\n"
          "  To set it up, type   wifi   here and press Enter: it will ask\r\n"
          "  for the network name and the password, try to connect, and\r\n"
          "  tell you whether it worked.\r\n"
          "==================================================================\r\n");
}

const char* describeSource(Source source) {
    switch (source) {
        case Source::Device: return "stored on this device";
        case Source::Firmware: return "compiled into the firmware";
        case Source::None: break;
    }
    return "not configured";
}

std::string currentIp() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info{};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &info) != ESP_OK || info.ip.addr == 0) return "";
    char ip[16];
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
    return ip;
}

void showStatus(State& s) {
    auto& wifi = WifiNetworkConnectionController::instance();
    std::string text;
    if (s.source == Source::None) {
        text = "WiFi: not configured. Type 'wifi' to set it up.\r\n";
    } else {
        text = "WiFi network: '" + s.inUse.ssid + "' (" + describeSource(s.source) + ")\r\n";
        if (wifi.connectedToNetwork()) {
            text += "Status: connected, IP " + currentIp() + "\r\n";
        } else {
            text += "Status: not connected\r\n";
        }
    }
    print(text);
}

void runAction(State& s, WifiSetupDialog::Action action);

void tryConnect(State& s) {
    auto& wifi = WifiNetworkConnectionController::instance();
    const WifiCredentials& p = s.dialog.pending();
    s.beforeAttempt = s.inUse;

    wifi.beginAttempt();
    wifi.applyCredentials(p.ssid, p.password);
    bool connected = false;
    ConnectFailure failure = ConnectFailure::Other;
    int reason = 0;
    std::string ip;

    if (!wifi.started() && !(s.startWifi && s.startWifi())) {
        failure = ConnectFailure::Other;
    } else {
        // One failure proves little: with a weak or busy access point the
        // first handshake often times out (reason 15, then a generic 205)
        // even with the right password, and the driver's own retry then
        // succeeds. So the controller keeps retrying for the whole
        // timeout, and the verdict is the most specific reason seen,
        // early only once the same specific reason has come back twice.
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CONFIG_CRONIOT_WIFI_CONNECT_TIMEOUT_S * 1000);
        int wrongPassword = 0;
        int notFound = 0;
        bool sawFailure = false;
        while (true) {
            TickType_t now = xTaskGetTickCount();
            if (now >= deadline) break;
            int r = 0;
            if (wifi.waitAttempt(pdTICKS_TO_MS(deadline - now), r, ip)) {
                connected = true;
                break;
            }
            if (r == 0) break;  // timed out
            reason = r;
            sawFailure = true;
            switch (classifyDisconnect(r)) {
                case ConnectFailure::WrongPassword: ++wrongPassword; break;
                case ConnectFailure::NetworkNotFound: ++notFound; break;
                default: break;
            }
            if (wrongPassword >= 2 || notFound >= 2) break;
        }
        if (wrongPassword > 0 && wrongPassword >= notFound) {
            failure = ConnectFailure::WrongPassword;
        } else if (notFound > 0) {
            failure = ConnectFailure::NetworkNotFound;
        } else {
            failure = sawFailure ? ConnectFailure::Other : ConnectFailure::Timeout;
        }
    }
    if (connected) {
        ESP_LOGI(TAG, "Joined '%s' from the console", p.ssid.c_str());
    } else {
        ESP_LOGW(TAG, "Could not join '%s' from the console (reason=%d)", p.ssid.c_str(), reason);
        // Quiet while the question is on screen: retrying a wrong
        // password would bury it under driver logs, and gains nothing.
        wifi.applyCredentials("", "");
        s.parked = true;
    }

    auto reply = s.dialog.onConnectResult(connected, failure);
    print(reply.text);
    if (connected && !ip.empty()) print("IP address: " + ip + "\r\n");
    runAction(s, reply.action);
}

void runAction(State& s, WifiSetupDialog::Action action) {
    using Action = WifiSetupDialog::Action;
    switch (action) {
        case Action::None:
            break;
        case Action::TryConnect:
            tryConnect(s);
            break;
        case Action::Save: {
            const WifiCredentials& p = s.dialog.pending();
            if (!Storage::instance().saveWifiCredentials(p)) {
                print("ERROR: the network works but could not be saved to the flash, so the device "
                      "will forget it when it restarts.\r\n");
                break;
            }
            if (!p.password.empty()) croniot::log::registerSecret(p.password);
            if (s.parked) {
                WifiNetworkConnectionController::instance().applyCredentials(p.ssid, p.password);
                s.parked = false;
            }
            s.inUse = p;
            s.source = Source::Device;
            ESP_LOGI(TAG, "WiFi network '%s' saved on the device", p.ssid.c_str());
            break;
        }
        case Action::Revert:
            WifiNetworkConnectionController::instance().applyCredentials(s.beforeAttempt.ssid,
                                                                         s.beforeAttempt.password);
            s.parked = false;
            break;
        case Action::Clear:
            if (!Storage::instance().clearWifiCredentials()) {
                print("ERROR: could not delete the stored network.\r\n");
                break;
            }
            ESP_LOGI(TAG, "Stored WiFi network forgotten");
            print("Forgotten. The device stays on its current network until it restarts; then it "
                  "uses the one compiled into the firmware, if any, or asks again.\r\n");
            break;
        case Action::ShowStatus:
            showStatus(s);
            break;
    }
}

void installInputs() {
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
    // The USB port of the board itself (/dev/ttyACM*). As a secondary
    // console it is output-only, so reading needs the driver.
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        esp_err_t err = usb_serial_jtag_driver_install(&cfg);
        if (err != ESP_OK) ESP_LOGW(TAG, "USB serial input unavailable: %s", esp_err_to_name(err));
    }
#endif
#if CONFIG_ESP_CONSOLE_UART
    // A USB-UART bridge on the console UART (/dev/ttyUSB*).
    if (!uart_is_driver_installed(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM))) {
        esp_err_t err = uart_driver_install(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM), 256, 0, 0,
                                            nullptr, 0);
        if (err != ESP_OK) ESP_LOGW(TAG, "UART input unavailable: %s", esp_err_to_name(err));
    }
#endif
}

int readInput(uint8_t* buf, size_t len) {
    int n = 0;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
    if (usb_serial_jtag_is_driver_installed()) {
        n = usb_serial_jtag_read_bytes(buf, len, kReadTicks);
        if (n > 0) return n;
    }
#endif
#if CONFIG_ESP_CONSOLE_UART
    if (uart_is_driver_installed(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM))) {
        n = uart_read_bytes(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM), buf, len, kReadTicks);
        if (n > 0) return n;
    }
#endif
    if (n <= 0) vTaskDelay(kReadTicks);  // no input path at all: don't spin
    return 0;
}

void consoleTask(void*) {
    State& s = *g_state;
    installInputs();

    TickType_t lastReminder = xTaskGetTickCount();
    uint8_t buf[32];
    std::string echo;

    while (true) {
        int n = readInput(buf, sizeof(buf));
        for (int i = 0; i < n; ++i) {
            auto line = s.editor.feed(static_cast<char>(buf[i]), echo);
            print(echo);
            if (!line) continue;

            auto reply = s.dialog.onLine(*line);
            print(reply.text);
            runAction(s, reply.action);
            s.editor.setMasked(s.dialog.expectsPassword());
            lastReminder = xTaskGetTickCount();
        }

        if (s.source == Source::None && !s.dialog.busy() &&
            xTaskGetTickCount() - lastReminder > pdMS_TO_TICKS(kReminderMs)) {
            print("WiFi is not configured: type 'wifi' and press Enter to set it up.\r\n");
            lastReminder = xTaskGetTickCount();
        }
    }
}

}  // namespace

void startWifiSetupConsole(Source source, const WifiCredentials& inUse, std::function<bool()> startWifi) {
    if (g_state != nullptr) return;
    g_state = new State();
    g_state->source = source;
    g_state->inUse = inUse;
    g_state->startWifi = std::move(startWifi);

    switch (source) {
        case Source::None:
            ESP_LOGW(TAG, "WiFi is not configured: waiting for 'wifi' on the serial console");
            printBanner();
            break;
        case Source::Firmware:
            ESP_LOGI(TAG, "Using the WiFi network compiled into the firmware ('%s')", inUse.ssid.c_str());
            print("WiFi: using the network compiled into the firmware. Type 'wifi' to store one on "
                  "this device instead.\r\n");
            break;
        case Source::Device:
            ESP_LOGI(TAG, "Using the WiFi network stored on the device ('%s')", inUse.ssid.c_str());
            break;
    }

    xTaskCreate(consoleTask, "croniot_wifi_cli", CONFIG_CRONIOT_WIFI_CONSOLE_TASK_STACK_SIZE, nullptr, 1,
                nullptr);
}

#else  // !CONFIG_CRONIOT_WIFI_CONSOLE

void startWifiSetupConsole(Source source, const WifiCredentials&, std::function<bool()>) {
    if (source == Source::None) {
        ESP_LOGE("WifiSetup", "WiFi is not configured and the setup console is disabled "
                              "(CONFIG_CRONIOT_WIFI_CONSOLE): the device can't get online");
    }
}

#endif

}  // namespace croniot::provision
