#ifndef CRONIOT_LOG_SEGMENTFILES_H
#define CRONIOT_LOG_SEGMENTFILES_H

#include <cstdint>
#include <cstring>
#include <optional>

namespace croniot::log {

// Pure file-name logic for Journal's segment files, `<prefix>_NNNNN.cbor`
// (see Journal::segmentPath()). No file I/O, so the "is this file one of
// this stream's segments?" decision behind orphan cleanup is testable on
// the host - getting it wrong would delete another stream's data, or the
// index itself.
class SegmentFiles {
public:
    // The segment id in `fileName` if it is exactly `<prefix>_<digits>.cbor`,
    // nullopt for anything else: another stream's segments, `<prefix>_index.bin`,
    // `<prefix>_index.tmp`, stray files.
    static std::optional<uint32_t> parseSegmentId(const char* fileName, const char* prefix) {
        size_t prefixLen = std::strlen(prefix);
        if (std::strncmp(fileName, prefix, prefixLen) != 0 || fileName[prefixLen] != '_') return std::nullopt;

        const char* digits = fileName + prefixLen + 1;
        const char* p = digits;
        uint64_t id = 0;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + static_cast<uint64_t>(*p - '0');
            if (id > UINT32_MAX) return std::nullopt;
            ++p;
        }
        if (p == digits || std::strcmp(p, ".cbor") != 0) return std::nullopt;
        return static_cast<uint32_t>(id);
    }
};

}  // namespace croniot::log

#endif
