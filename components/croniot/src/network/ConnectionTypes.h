#pragma once
#include <functional>
#include <string>

namespace connection {
    using WifiConnectedCallback = std::function<void(const std::string& ssid)>;
    using ClientAuthenticated = std::function<void(const std::string& message)>;
}
  