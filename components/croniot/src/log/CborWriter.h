#ifndef CRONIOT_LOG_CBORWRITER_H
#define CRONIOT_LOG_CBORWRITER_H

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "LogRecord.h"

namespace croniot::log {

// Minimal hand-written CBOR (RFC 8949) encoder: just arrays, maps,
// unsigned/signed integers and text strings - everything the plan's log
// records and batch headers need, nothing else. No dependency added; a
// full CBOR library would be overkill for ~5 tag types.
class CborWriter {
public:
    void writeArrayHeader(size_t count) { writeTypeAndLength(0x80, count); }
    void writeMapHeader(size_t count) { writeTypeAndLength(0xA0, count); }

    void writeUnsigned(uint64_t value) { writeTypeAndLength(0x00, value); }

    void writeSigned(int64_t value) {
        if (value >= 0) {
            writeUnsigned(static_cast<uint64_t>(value));
        } else {
            writeTypeAndLength(0x20, static_cast<uint64_t>(-1 - value));
        }
    }

    void writeString(const std::string& value) {
        writeTypeAndLength(0x60, value.size());
        buf_.insert(buf_.end(), value.begin(), value.end());
    }

    void writeBool(bool value) { buf_.push_back(value ? 0xF5 : 0xF4); }
    void writeNull() { buf_.push_back(0xF6); }

    // Major type 7, additional info 27: IEEE 754 double-precision float
    // (RFC 8949 §3.3). Added for sensor readings (plan §7.2's batch
    // shape) - log records never needed a real number type, only
    // integers and strings, until now.
    void writeDouble(double value) {
        buf_.push_back(0xFB);
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        appendBigEndian(bits, 8);
    }

    const std::vector<uint8_t>& bytes() const { return buf_; }

private:
    // majorType is already shifted into the top 3 bits (e.g. 0x80 for
    // arrays); this appends the correct additional-info encoding for
    // `length` (which doubles as the integer value for major types 0/1).
    void writeTypeAndLength(uint8_t majorType, uint64_t length) {
        if (length < 24) {
            buf_.push_back(majorType | static_cast<uint8_t>(length));
        } else if (length <= 0xFF) {
            buf_.push_back(majorType | 24);
            buf_.push_back(static_cast<uint8_t>(length));
        } else if (length <= 0xFFFF) {
            buf_.push_back(majorType | 25);
            appendBigEndian(length, 2);
        } else if (length <= 0xFFFFFFFFULL) {
            buf_.push_back(majorType | 26);
            appendBigEndian(length, 4);
        } else {
            buf_.push_back(majorType | 27);
            appendBigEndian(length, 8);
        }
    }

    void appendBigEndian(uint64_t value, int byteCount) {
        for (int i = byteCount - 1; i >= 0; --i) {
            buf_.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
        }
    }

    std::vector<uint8_t> buf_;
};

// Wire shape from the plan: [seq, uptime_ms, level, tag, msg].
inline std::vector<uint8_t> encodeLogRecord(const LogRecord& record) {
    CborWriter writer;
    writer.writeArrayHeader(5);
    writer.writeUnsigned(record.seq);
    writer.writeUnsigned(record.uptimeMs);
    writer.writeUnsigned(static_cast<uint64_t>(record.level));
    writer.writeString(record.tag);
    writer.writeString(record.message);
    return writer.bytes();
}

struct BatchHeader {
    uint8_t version = 1;
    uint32_t bootId = 0;
    std::string firmwareVersion;
    std::string elfSha256Prefix;  // first 8 hex chars, per the plan
    uint64_t nowMs = 0;
    std::optional<uint64_t> epochMs;  // only present once SNTP has run
    uint32_t dropped = 0;
};

// Wire shape from the plan: {v, boot, fw, elf, now_ms, epoch_ms?, dropped}.
inline std::vector<uint8_t> encodeBatchHeader(const BatchHeader& header) {
    CborWriter writer;
    writer.writeMapHeader(header.epochMs ? 7 : 6);

    writer.writeString("v");
    writer.writeUnsigned(header.version);

    writer.writeString("boot");
    writer.writeUnsigned(header.bootId);

    writer.writeString("fw");
    writer.writeString(header.firmwareVersion);

    writer.writeString("elf");
    writer.writeString(header.elfSha256Prefix);

    writer.writeString("now_ms");
    writer.writeUnsigned(header.nowMs);

    if (header.epochMs) {
        writer.writeString("epoch_ms");
        writer.writeUnsigned(*header.epochMs);
    }

    writer.writeString("dropped");
    writer.writeUnsigned(header.dropped);

    return writer.bytes();
}

}  // namespace croniot::log

#endif
