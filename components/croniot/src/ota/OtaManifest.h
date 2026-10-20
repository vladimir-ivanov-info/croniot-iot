#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace croniot::ota {

// What the server publishes, retained, on /server/<uuid>/ota to ask this
// device to install a firmware release. See CRONIOT.md §5 for the
// contract and croniot-kmp's OtaService for the producer:
//
//   {"requestId":"…","version":"1.4.0","path":"/api/iot/firmware/12",
//    "size":1960112,"sha256":"<64 hex>","port":8090}
//
// There is deliberately no absolute URL: the device always downloads
// from the server address it was configured with (CroniotConfig::remote),
// so whoever can publish on this topic can at most point it at another
// path or port of that same server, never at a host of their choosing.
// `port` is optional (0 = the configured HTTP port).
struct Manifest {
    std::string requestId;
    std::string version;
    std::string path;
    uint16_t port = 0;
    uint32_t size = 0;
    std::string sha256;  // lowercase hex
};

// Longest version that fits esp_app_desc_t::version (32 bytes with NUL).
inline constexpr size_t kMaxVersionLen = 31;
// The ESP32-C6 can map at most 16 MB of flash, so no app image is bigger.
inline constexpr uint32_t kMaxImageSize = 16u * 1024 * 1024;

// nullopt for anything that is not a complete, valid manifest - including
// the empty retained payload the server publishes to clear a request.
std::optional<Manifest> parseManifest(const std::string& json);

}  // namespace croniot::ota
