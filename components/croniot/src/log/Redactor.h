#ifndef CRONIOT_LOG_REDACTOR_H
#define CRONIOT_LOG_REDACTOR_H

#include <string>
#include <vector>

namespace croniot::log {

// Runtime defensive filter: scrubs any registered secret *value* out of an
// already-formatted line before it reaches a sink, regardless of which
// code path produced the line. Complements (doesn't replace) the
// call-site helper croniot::redact() from Redact.h - that one masks a
// value the programmer already knows is sensitive at the point of
// logging; this one catches it anyway if it leaked through some other
// path (third-party code, a copy-pasted error message, ...).
class Redactor {
public:
    void registerSecret(const std::string& secret) {
        if (secret.empty()) return;
        for (const auto& existing : secrets_) {
            if (existing == secret) return;  // no duplicates
        }
        secrets_.push_back(secret);
    }

    void clear() { secrets_.clear(); }

    std::string redact(const std::string& line) const {
        if (secrets_.empty()) return line;

        std::string result = line;
        for (const auto& secret : secrets_) {
            if (secret.empty()) continue;
            size_t pos = 0;
            while ((pos = result.find(secret, pos)) != std::string::npos) {
                result.replace(pos, secret.size(), "***");
                pos += 3;
            }
        }
        return result;
    }

private:
    std::vector<std::string> secrets_;
};

}  // namespace croniot::log

#endif
