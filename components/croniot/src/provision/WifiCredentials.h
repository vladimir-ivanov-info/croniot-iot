#pragma once

#include <optional>
#include <string>

namespace croniot::provision {

// The WiFi network this device joins. Stored on the device itself, in
// /littlefs/wifi.json, instead of being compiled into the firmware: the
// same .bin then works on any board, can be built by CI and installed
// over the air without carrying anyone's password. Set from the serial
// console (WifiSetupConsole) and read once at boot (RemoteChannel).
//
//   {"ssid":"MyNetwork","password":"…"}
struct WifiCredentials {
    std::string ssid;
    std::string password;  // empty = open network

    bool operator==(const WifiCredentials& o) const { return ssid == o.ssid && password == o.password; }
};

// 802.11 limits: an SSID is 1-32 bytes; a WPA2/WPA3 passphrase is 8-63
// printable characters, or exactly 64 hex digits (a raw PSK).
constexpr size_t kMaxSsidLen = 32;
constexpr size_t kMinPasswordLen = 8;
constexpr size_t kMaxPasswordLen = 64;

// nullopt when valid, otherwise a sentence for the person typing it.
std::optional<std::string> ssidError(const std::string& ssid);
std::optional<std::string> passwordError(const std::string& password);

// nullopt for anything malformed or invalid: a damaged file must read as
// "not configured" (and say so on the console), never as a half-read
// network name the device then keeps failing to join.
std::optional<WifiCredentials> parseWifiJson(const std::string& json);
std::string encodeWifiJson(const WifiCredentials& credentials);

// Where the credentials in use came from, for the console and the log.
enum class Source {
    None,      // nothing anywhere: WiFi stays off until someone sets it
    Device,    // /littlefs/wifi.json
    Firmware,  // CroniotConfig::remote, i.e. compiled in (e.g. secrets.h)
};

// The device's own file wins: it is what the person last typed on this
// very board, while the compiled-in values are only a development
// convenience that every board built from that tree shares.
Source resolve(const std::optional<WifiCredentials>& stored, const WifiCredentials& compiledIn,
               WifiCredentials& out);

// Why a connection attempt failed, from the ESP-IDF disconnect reason
// (wifi_err_reason_t), in the terms the person can act on.
enum class ConnectFailure {
    WrongPassword,
    NetworkNotFound,
    Timeout,
    Other,
};
ConnectFailure classifyDisconnect(int reason);
const char* describe(ConnectFailure failure);

// Turns the disconnect reasons of one attempt into a single verdict.
// One failure proves little: with a weak or busy access point the first
// handshake often times out (reason 15, then a generic 205) even with
// the right password, and the driver's next try then succeeds. So the
// verdict is final early only once the same specific kind has come back
// twice; otherwise the caller keeps waiting until its timeout, and the
// result is the most specific kind seen.
class ConnectVerdict {
public:
    // Feeds one disconnect reason. Returns true when waiting longer
    // would not change the verdict.
    bool onFailure(int reason);
    // Timeout when no failure was reported at all.
    ConnectFailure result() const;

private:
    int wrongPassword_ = 0;
    int notFound_ = 0;
    bool sawFailure_ = false;
};

}  // namespace croniot::provision
