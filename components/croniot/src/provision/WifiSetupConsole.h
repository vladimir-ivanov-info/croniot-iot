#pragma once

#include <functional>

#include "WifiCredentials.h"

namespace croniot::provision {

// The serial-console side of WiFi setup: reads what the person types in
// a serial monitor (`idf.py monitor`, or the one in VSCode), runs the
// WifiSetupDialog, and carries out its actions against the WiFi driver
// and /littlefs/wifi.json. Started by RemoteChannel when the transport
// is WiFi, whether or not credentials already exist, so the network can
// be changed at any time by typing `wifi`.
//
// `source` and `inUse` are what RemoteChannel resolved at boot.
// `startWifi` brings the WiFi driver up for the first time; it is only
// needed when source == Source::None, because then nothing started it.
//
// No-op when CONFIG_CRONIOT_WIFI_CONSOLE is off.
void startWifiSetupConsole(Source source, const WifiCredentials& inUse, std::function<bool()> startWifi);

}  // namespace croniot::provision
