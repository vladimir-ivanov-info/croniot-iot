#include "HealthReport.h"

#include <cinttypes>
#include <cstdio>

namespace croniot::health {

namespace {

std::string percentField(const char* key, int percent) {
    char buf[24];
    if (percent == kNoPercent) {
        std::snprintf(buf, sizeof(buf), "%s=na", key);
    } else {
        std::snprintf(buf, sizeof(buf), "%s=%d", key, percent);
    }
    return buf;
}

}  // namespace

std::string encodeMemNet(const MemNetInputs& in) {
    char rssiBuf[16];
    if (in.rssi == kNoRssi) {
        std::snprintf(rssiBuf, sizeof(rssiBuf), "na");
    } else {
        std::snprintf(rssiBuf, sizeof(rssiBuf), "%d", in.rssi);
    }

    char buf[160];
    std::snprintf(buf, sizeof(buf), "heap=%" PRIu32 " blk=%" PRIu32 " up=%" PRIu64 " rssi=%s recon=%" PRIu32
                  " drop=%" PRIu32,
                  in.freeHeapBytes, in.largestFreeBlockBytes, in.uptimeSeconds, rssiBuf, in.wifiReconnects,
                  in.logsDropped);
    return buf;
}

std::string encodeStorage(const StorageInputs& in) {
    std::string out;
    out += percentField("jfree", in.journalFreePercent);
    out += ' ';
    out += percentField("afree", in.archiveFreePercent);
    out += ' ';
    out += percentField("jwear", in.journalWearPercent);
    out += ' ';
    out += percentField("awear", in.archiveWearPercent);

    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  " lbl=%" PRIu32 " ebl=%" PRIu32 " dbl=%" PRIu32 " lrs=%" PRIu32 " ers=%" PRIu32 " drs=%" PRIu32,
                  in.backlogCount[0], in.backlogCount[1], in.backlogCount[2], in.resendAttempts[0],
                  in.resendAttempts[1], in.resendAttempts[2]);
    out += buf;
    return out;
}

}  // namespace croniot::health
