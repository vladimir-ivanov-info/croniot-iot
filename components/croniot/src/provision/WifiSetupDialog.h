#pragma once

#include <optional>
#include <string>

#include "WifiCredentials.h"

namespace croniot::provision {

// Turns the bytes typed into a serial monitor into lines. A monitor sends
// each key as it is pressed and does not echo it, so the echo is this
// class's job: the typed character, or '*' while a password is typed.
// Backspace (0x08 or 0x7f) erases; Enter may arrive as CR, LF or CRLF.
class LineEditor {
public:
    void setMasked(bool masked) { masked_ = masked; }

    // Feeds one byte. Returns a finished line on Enter; `echo` gets what
    // to write back so the person sees what they type.
    std::optional<std::string> feed(char c, std::string& echo);

private:
    std::string line_;
    bool masked_ = false;
    bool lastWasCr_ = false;
};

// The console conversation that sets the WiFi network, without any I/O:
// it takes finished lines and connection results, and answers with text
// to print and an action for WifiSetupConsole to carry out. Kept apart
// so that every prompt and every branch is covered by host tests.
//
//   > wifi
//   Network name (SSID): MyNetwork
//   Password (leave empty for an open network): ********
//   Connecting to 'MyNetwork'...
//   OK: connected to 'MyNetwork'. Saved on the device.
class WifiSetupDialog {
public:
    enum class Action {
        None,
        TryConnect,  // join pending(), then call onConnectResult()
        Save,        // store pending() in /littlefs/wifi.json
        Revert,      // go back to the network in use before TryConnect
        Clear,       // delete /littlefs/wifi.json
        ShowStatus,
    };

    struct Reply {
        std::string text;
        Action action = Action::None;
    };

    Reply onLine(const std::string& line);
    Reply onConnectResult(bool connected, ConnectFailure failure);

    // What the person typed, for TryConnect and Save.
    const WifiCredentials& pending() const { return pending_; }
    // Whether the next line is a password, so the editor masks it.
    bool expectsPassword() const { return state_ == State::AskPassword; }
    bool busy() const { return state_ != State::Idle; }

    static const char* usage();

private:
    enum class State {
        Idle,
        AskSsid,
        AskPassword,
        Connecting,
        AskSaveAnyway,
    };

    State state_ = State::Idle;
    WifiCredentials pending_;
};

}  // namespace croniot::provision
