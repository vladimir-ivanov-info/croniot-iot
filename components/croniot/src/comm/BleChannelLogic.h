#pragma once

#include <cstdint>
#include <string>

// Pure, hardware-free logic extracted from BleChannel so it can be
// unit-tested on host without NimBLE/FreeRTOS/ESP-IDF. BleChannel's public
// static parseStaticPasskey()/djb2Hash() delegate here.
namespace croniot {
namespace BleChannelLogic {

// A valid BLE static passkey is exactly 6 numeric ASCII digits (000000-999999).
// Returns false (outPasskey left unchanged) for any other input.
bool parseStaticPasskey(const std::string& password, uint32_t& outPasskey);

uint32_t djb2Hash(const std::string& s);

} // namespace BleChannelLogic
} // namespace croniot
