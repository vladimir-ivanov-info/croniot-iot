#include "OtaManifest.h"

#include <cctype>

#include "JsonScanner.h"

namespace croniot::ota {

namespace {

bool isValidSha256(std::string& hex) {
    if (hex.size() != 64) return false;
    for (char& c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return true;
}

// A server path, not a URL: "/api/iot/firmware/12". Rejecting "://" and
// "//" keeps it from smuggling in a scheme or a host.
bool isValidPath(const std::string& path) {
    if (path.empty() || path.size() > 200 || path[0] != '/') return false;
    if (path.find("//") != std::string::npos) return false;
    for (char c : path) {
        if (c <= ' ' || c == 0x7f) return false;
    }
    return true;
}

}  // namespace

std::optional<Manifest> parseManifest(const std::string& json) {
    JsonScanner scanner(json);
    if (!scanner.consume('{')) return std::nullopt;

    Manifest m;
    bool hasSize = false;

    if (scanner.peek() != '}') {
        while (true) {
            auto key = scanner.parseString();
            if (!key || !scanner.ok() || !scanner.consume(':')) return std::nullopt;

            if (*key == "requestId" || *key == "version" || *key == "path" || *key == "sha256") {
                auto value = scanner.parseString();
                if (!value || !scanner.ok()) return std::nullopt;
                if (*key == "requestId") m.requestId = *value;
                else if (*key == "version") m.version = *value;
                else if (*key == "path") m.path = *value;
                else m.sha256 = *value;
            } else if (*key == "size" || *key == "port") {
                auto value = scanner.parseInt();
                if (!value || !scanner.ok()) return std::nullopt;
                if (*key == "size") {
                    if (*value <= 0 || *value > kMaxImageSize) return std::nullopt;
                    m.size = static_cast<uint32_t>(*value);
                    hasSize = true;
                } else {
                    if (*value < 0 || *value > 65535) return std::nullopt;
                    m.port = static_cast<uint16_t>(*value);
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

    if (m.requestId.empty() || m.requestId.size() > 64) return std::nullopt;
    if (m.version.empty() || m.version.size() > kMaxVersionLen) return std::nullopt;
    if (!isValidPath(m.path) || !hasSize || !isValidSha256(m.sha256)) return std::nullopt;
    return m;
}

}  // namespace croniot::ota
