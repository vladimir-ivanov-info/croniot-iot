#include "WifiSetupDialog.h"

#include <cctype>

namespace croniot::provision {

namespace {

constexpr const char* kAskSsid = "Network name (SSID): ";
constexpr const char* kAskPassword = "Password (leave empty for an open network): ";
constexpr const char* kAskSaveAnyway = "Save it anyway, to use it once that network is in range? [y/N]: ";

std::string trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

std::optional<std::string> LineEditor::feed(char c, std::string& echo) {
    echo.clear();
    bool wasCr = lastWasCr_;
    lastWasCr_ = (c == '\r');

    if (c == '\r' || c == '\n') {
        if (c == '\n' && wasCr) return std::nullopt;  // the LF of a CRLF
        echo = "\r\n";
        std::string done = line_;
        line_.clear();
        return done;
    }
    if (c == 0x08 || c == 0x7f) {
        if (!line_.empty()) {
            // UTF-8: drop the whole last character (its continuation
            // bytes and then its lead byte), not one byte of it.
            while (!line_.empty() && (static_cast<unsigned char>(line_.back()) & 0xC0) == 0x80) {
                line_.pop_back();
            }
            if (!line_.empty()) line_.pop_back();
            echo = "\b \b";
        }
        return std::nullopt;
    }
    if (static_cast<unsigned char>(c) < 0x20) return std::nullopt;  // other control keys
    if (line_.size() >= 128) return std::nullopt;                      // nothing valid is this long

    line_ += c;
    // A multi-byte character is echoed once, on its first byte.
    bool continuation = (static_cast<unsigned char>(c) & 0xC0) == 0x80;
    if (masked_) {
        if (!continuation) echo = "*";
    } else {
        echo = std::string(1, c);
    }
    return std::nullopt;
}

const char* WifiSetupDialog::usage() {
    return "WiFi commands:\r\n"
           "  wifi          set the WiFi network (asks for name and password)\r\n"
           "  wifi status   show the network in use and whether it is connected\r\n"
           "  wifi clear    forget the network stored on this device\r\n";
}

WifiSetupDialog::Reply WifiSetupDialog::onLine(const std::string& raw) {
    switch (state_) {
        case State::Idle: {
            std::string cmd = lower(trim(raw));
            if (cmd.empty()) return {};
            if (cmd == "wifi" || cmd == "wifi set") {
                state_ = State::AskSsid;
                pending_ = {};
                return {kAskSsid};
            }
            if (cmd == "wifi status") return {"", Action::ShowStatus};
            if (cmd == "wifi clear") return {"", Action::Clear};
            return {std::string("Unknown command '") + trim(raw) + "'.\r\n" + usage()};
        }

        case State::AskSsid: {
            // Not trimmed: a network name may really start or end with a
            // space. Only an empty line cancels.
            if (raw.empty()) {
                state_ = State::Idle;
                return {"Cancelled. Nothing was changed.\r\n"};
            }
            if (auto error = ssidError(raw)) return {*error + "\r\n" + kAskSsid};
            pending_.ssid = raw;
            state_ = State::AskPassword;
            return {kAskPassword};
        }

        case State::AskPassword: {
            if (auto error = passwordError(raw)) return {*error + "\r\n" + kAskPassword};
            pending_.password = raw;
            state_ = State::Connecting;
            return {"Connecting to '" + pending_.ssid + "'...\r\n", Action::TryConnect};
        }

        case State::Connecting:
            return {};  // the result is on its way; typing doesn't change it

        case State::AskSaveAnyway: {
            std::string answer = lower(trim(raw));
            if (answer == "y" || answer == "yes" || answer == "s" || answer == "si" || answer == "sí") {
                state_ = State::Idle;
                // WifiSetupConsole says whether the write worked.
                return {"", Action::Save};
            }
            state_ = State::Idle;
            return {"Not saved: the device keeps the network it had before. "
                    "Type 'wifi' to try again.\r\n",
                    Action::Revert};
        }
    }
    return {};
}

WifiSetupDialog::Reply WifiSetupDialog::onConnectResult(bool connected, ConnectFailure failure) {
    if (state_ != State::Connecting) return {};
    if (connected) {
        state_ = State::Idle;
        // Saving comes next; WifiSetupConsole says whether the write worked.
        return {"OK: connected to '" + pending_.ssid + "'.\r\n", Action::Save};
    }
    state_ = State::AskSaveAnyway;
    return {std::string("FAILED: could not connect to '") + pending_.ssid + "': " + describe(failure) +
            "\r\n" + kAskSaveAnyway};
}

}  // namespace croniot::provision
