#ifndef CRONIOT_LOG_REDACTOR_H
#define CRONIOT_LOG_REDACTOR_H

#include <cstring>
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

    // Host-side/off-hot-path use only - allocates a new std::string per
    // call. See redactInPlace() for why the on-device hook can't use this.
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

    // Zero-allocation counterpart to redact(), for the on-device hot path
    // (LogRouter::vprintfHook, operating directly on LogRecord::message).
    // `buf` is a NUL-terminated C string of capacity `bufSize`; every
    // match is overwritten with "***" in place via memmove, no heap
    // allocation. If a registered secret is shorter than "***" (under 3
    // chars - not a realistic secret length, but handled rather than
    // risking an overflow), the replacement's growth is clamped to what
    // still fits in `bufSize` instead of writing past the buffer.
    void redactInPlace(char* buf, size_t bufSize) const {
        if (secrets_.empty() || bufSize == 0) return;
        constexpr char kMask[] = "***";
        constexpr size_t kMaskLen = 3;

        for (const auto& secret : secrets_) {
            if (secret.empty()) continue;
            size_t secretLen = secret.size();
            size_t len = std::strlen(buf);
            size_t pos = 0;
            while (pos + secretLen <= len) {
                if (std::memcmp(buf + pos, secret.data(), secretLen) != 0) {
                    ++pos;
                    continue;
                }
                size_t maskLen = kMaskLen;
                if (kMaskLen > secretLen) {
                    size_t growth = kMaskLen - secretLen;
                    size_t available = bufSize - 1 - len;
                    if (growth > available) maskLen = secretLen + available;
                }
                size_t tailLen = len - (pos + secretLen);
                std::memmove(buf + pos + maskLen, buf + pos + secretLen, tailLen);
                std::memcpy(buf + pos, kMask, maskLen);
                len = pos + maskLen + tailLen;
                buf[len] = '\0';
                pos += maskLen;
            }
        }
    }

private:
    std::vector<std::string> secrets_;
};

}  // namespace croniot::log

#endif
