#ifndef CRONIOT_REDACT_H
#define CRONIOT_REDACT_H

#include <algorithm>
#include <string>

namespace croniot {

// Masks a secret for logging: keeps a short prefix and the total length,
// drops the rest. Enough to confirm "a token/passkey was used" without
// exposing the value in a console, a shared log, or a bug report.
inline std::string redact(const std::string& secret) {
    if (secret.empty()) {
        return "(empty)";
    }
    constexpr size_t kPrefixLen = 4;
    std::string prefix = secret.substr(0, std::min(kPrefixLen, secret.size()));
    return prefix + "...(" + std::to_string(secret.size()) + ")";
}

}  // namespace croniot

#endif
