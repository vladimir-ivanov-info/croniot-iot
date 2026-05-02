#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace croniot {

enum class ChannelType {
    Remote,
    Ble,
};

enum class RemoteTransport {
    Wifi,
    Sim7600,
};

struct CroniotConfig {
    std::string deviceUuid;
    std::string deviceName;
    std::string deviceDescription;

    std::string accountEmail;
    std::string accountUuid;
    std::string accountPassword;

    std::vector<ChannelType> channels;

    struct RemoteCfg {
        RemoteTransport transport = RemoteTransport::Wifi;
        std::string serverAddress;
        uint16_t serverHttpPort = 8090;
        uint16_t serverMqttPort = 1883;

        std::string wifiSsid;
        std::string wifiPassword;

        std::string simPin;
        std::string simApn;
    } remote;

    struct BleCfg {
        std::string localName;
        std::string password;
        bool securityEnabled = false;
    } ble;
};

}
