#include "RemoteLogConfig.h"

#include "JsonScanner.h"

namespace croniot::log {

namespace {

using croniot::JsonScanner;

std::optional<Level> parseLevelName(const std::string& name) {
    if (name == "error") return Level::Error;
    if (name == "warn") return Level::Warn;
    if (name == "info") return Level::Info;
    if (name == "debug") return Level::Debug;
    if (name == "trace") return Level::Trace;
    return std::nullopt;
}

}  // namespace

std::optional<RemoteLogConfig> parseRemoteLogConfig(const std::string& json) {
    JsonScanner scanner(json);
    if (!scanner.consume('{')) return std::nullopt;

    RemoteLogConfig config;

    if (scanner.peek() != '}') {
        while (true) {
            auto key = scanner.parseString();
            if (!key || !scanner.ok() || !scanner.consume(':')) return std::nullopt;

            if (*key == "default") {
                auto value = scanner.parseString();
                if (!value || !scanner.ok()) return std::nullopt;
                auto level = parseLevelName(*value);
                if (!level) return std::nullopt;
                config.defaultLevel = level;
            } else if (*key == "tags") {
                if (!scanner.consume('{')) return std::nullopt;
                if (scanner.peek() != '}') {
                    while (true) {
                        auto tag = scanner.parseString();
                        if (!tag || !scanner.ok() || !scanner.consume(':')) return std::nullopt;
                        auto value = scanner.parseString();
                        if (!value || !scanner.ok()) return std::nullopt;
                        auto level = parseLevelName(*value);
                        if (!level) return std::nullopt;
                        config.tagLevels.emplace_back(*tag, *level);
                        if (scanner.peek() == ',') {
                            scanner.consume(',');
                            continue;
                        }
                        break;
                    }
                }
                if (!scanner.consume('}')) return std::nullopt;
            } else if (*key == "ttlSec") {
                auto value = scanner.parseInt();
                if (!value || !scanner.ok() || *value < 0) return std::nullopt;
                config.ttlSec = static_cast<uint32_t>(*value);
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
    return config;
}

}  // namespace croniot::log
