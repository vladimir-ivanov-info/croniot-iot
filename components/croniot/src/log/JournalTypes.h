#ifndef CRONIOT_LOG_JOURNALTYPES_H
#define CRONIOT_LOG_JOURNALTYPES_H

#include <cstdint>
#include <string>

namespace croniot::log {

// The three journal streams from the plan (§8.1/§8.9): `Logs` and
// `Events` share the `journal` LittleFS partition, `Data` lives in the
// smaller `archive` partition. `Events` is special everywhere below - it
// carries incidents and structured events, and is never subject to space
// reclamation (see SpaceReclaimer.h).
enum class Stream : uint8_t { Logs = 0, Events = 1, Data = 2 };

inline const char* toString(Stream stream) {
    switch (stream) {
        case Stream::Logs: return "logs";
        case Stream::Events: return "events";
        case Stream::Data: return "data";
    }
    return "unknown";
}

// An explicit, reported hole in a stream's sequence: `[fromSeq, toSeq)`
// worth of records were reclaimed before the server (or, over BLE, the
// phone - see plan §5 point 11) ever acknowledged them. Never invented
// implicitly - see JournalCursor::reclaimTo().
struct GapMarker {
    Stream stream;
    uint32_t fromSeq;
    uint32_t toSeq;  // exclusive
    uint32_t count;
    std::string reason;
};

}  // namespace croniot::log

#endif
