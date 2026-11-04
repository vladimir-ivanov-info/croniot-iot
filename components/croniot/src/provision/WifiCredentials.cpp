#include "WifiCredentials.h"

#include <cctype>

#include "JsonScanner.h"

namespace croniot::provision {

namespace {

bool hasControlChars(const std::string& s) {
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7f) return true;
    }
    return false;
}

bool isHex(const std::string& s) {
    for (unsigned char c : s) {
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

// Only the escapes JsonScanner::parseString() reads back. Control
// characters never get here: ssidError()/passwordError() reject them.
std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

}  // namespace

std::optional<std::string> ssidError(const std::string& ssid) {
    if (ssid.empty()) return std::string("The network name can't be empty.");
    if (ssid.size() > kMaxSsidLen) return std::string("A network name has at most 32 characters.");
    if (hasControlChars(ssid)) return std::string("The network name has a character that can't be typed.");
    return std::nullopt;
}

std::optional<std::string> passwordError(const std::string& password) {
    if (password.empty()) return std::nullopt;  // open network
    if (hasControlChars(password)) return std::string("The password has a character that can't be typed.");
    if (password.size() < kMinPasswordLen) return std::string("A WiFi password has at least 8 characters.");
    if (password.size() > kMaxPasswordLen) return std::string("A WiFi password has at most 63 characters.");
    if (password.size() == kMaxPasswordLen && !isHex(password)) {
        return std::string("A WiFi password has at most 63 characters.");
    }
    return std::nullopt;
}

std::optional<WifiCredentials> parseWifiJson(const std::string& json) {
    JsonScanner scanner(json);
    if (!scanner.consume('{')) return std::nullopt;

    WifiCredentials c;
    bool hasSsid = false;

    if (scanner.peek() != '}') {
        while (true) {
            auto key = scanner.parseString();
            if (!key || !scanner.ok() || !scanner.consume(':')) return std::nullopt;

            if (*key == "ssid" || *key == "password") {
                auto value = scanner.parseString();
                if (!value || !scanner.ok()) return std::nullopt;
                if (*key == "ssid") {
                    c.ssid = *value;
                    hasSsid = true;
                } else {
                    c.password = *value;
                }
            } else {
                scanner.skipValue();
                if (!scanner.ok()) return std::nullopt;
            }

            if (scanner.peek() == ',') {
                scanner.consume(',');
                continue;
            }
            break;
        }
    }
    if (!scanner.consume('}')) return std::nullopt;

    if (!hasSsid || ssidError(c.ssid) || passwordError(c.password)) return std::nullopt;
    return c;
}

std::string encodeWifiJson(const WifiCredentials& credentials) {
    return "{\"ssid\":" + quoted(credentials.ssid) + ",\"password\":" + quoted(credentials.password) + "}";
}

Source resolve(const std::optional<WifiCredentials>& stored, const WifiCredentials& compiledIn,
               WifiCredentials& out) {
    if (stored) {
        out = *stored;
        return Source::Device;
    }
    if (!compiledIn.ssid.empty()) {
        out = compiledIn;
        return Source::Firmware;
    }
    out = {};
    return Source::None;
}

ConnectFailure classifyDisconnect(int reason) {
    switch (reason) {
        case 2:    // AUTH_EXPIRE
        case 14:   // MIC_FAILURE
        case 15:   // 4WAY_HANDSHAKE_TIMEOUT
        case 202:  // AUTH_FAIL
        case 204:  // HANDSHAKE_TIMEOUT
            return ConnectFailure::WrongPassword;
        case 201:  // NO_AP_FOUND
        case 210:  // NO_AP_FOUND_W_COMPATIBLE_SECURITY
        case 211:  // NO_AP_FOUND_IN_AUTHMODE_THRESHOLD
        case 212:  // NO_AP_FOUND_IN_RSSI_THRESHOLD
            return ConnectFailure::NetworkNotFound;
        default:
            return ConnectFailure::Other;
    }
}

const char* describe(ConnectFailure failure) {
    switch (failure) {
        case ConnectFailure::WrongPassword:
            return "the network rejected the password. Check it (upper and lower case matter); "
                   "if it is right, the signal may be too weak where the device is.";
        case ConnectFailure::NetworkNotFound:
            return "no network with that name is in range. Check the name (upper and lower case "
                   "matter) and that it is a 2.4 GHz network: this chip can't see 5 GHz ones.";
        case ConnectFailure::Timeout:
            return "the network didn't answer in time.";
        case ConnectFailure::Other:
            break;
    }
    return "the network refused the connection.";
}

}  // namespace croniot::provision
