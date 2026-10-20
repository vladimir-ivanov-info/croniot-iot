#pragma once

#include "CroniotConfig.h"

// SDK-internal entry points of the OTA flow, called from CommonSetup.
// Projects use ota/Ota.h instead.
namespace croniot::ota {

// Reads the persisted update record, works out whether this boot is a
// normal one, a new firmware under validation, a rollback or a probe of
// the previous firmware, and starts the croniot_ota task. Call early in
// CommonSetup::setup(), before anything that can fail to connect: the
// validation deadline counts from boot.
void begin(const CroniotConfig::RemoteCfg& remote);

// The server accepted this device's registration on this boot.
void onServerAuthenticated();

// Subscribes to /server/<uuid>/ota. Call once messaging has started.
void listen();

}  // namespace croniot::ota
