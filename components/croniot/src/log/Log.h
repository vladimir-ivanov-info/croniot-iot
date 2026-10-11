#ifndef CRONIOT_LOG_LOG_H
#define CRONIOT_LOG_LOG_H

#include <initializer_list>
#include <map>
#include <string>
#include <utility>

#include "ConsoleSink.h"
#include "Detail.h"
#include "Level.h"
#include "Result.h"

namespace croniot::log {

// Where a record can be rendered. Only Console does anything in PR7 -
// Flash (PR9) and Sd (PR21) are accepted here now purely so call sites
// that reference them already compile against the final API shape; they
// are no-op placeholders until their respective PRs land.
enum class Sink { Console, Flash, Sd };

// Energy/latency profile for the *drain* side (how eagerly queued records
// get rendered/uplinked). Stored by init() and nothing else - Fase 3
// (telemetry) is what will actually branch on this later. Do not assume
// PR7 changes behavior based on this field; it's a config placeholder,
// same spirit as Sink::Flash/Sink::Sd above.
enum class Profile { RealTime, Batched, Minimal };

struct LogConfig {
    Level capture = Level::Info;
    Profile profile = Profile::RealTime;
    std::map<std::string, Level> tags;  // fed into LevelResolver::setCodeTagLevel
    ConsoleFormat consoleFormat = ConsoleFormat::Text;
};

// Installs the esp_log hook, starts the drain task, and applies `config`.
// Safe to call with no arguments for the Nivel 0/default behavior
// (console, Text, Info). A no-op if CONFIG_CRONIOT_LOG_ENABLE=n (the
// module's kill switch) - no hook installed, no task created.
void init(const LogConfig& config = {});

// Routes straight into the ring (RTC store too - see RtcCriticalStore.h),
// tagged as an event rather than an ESP_LOGx-sourced line, and - unlike a
// normal log line - never rate-limited: events are rare and always
// semantically distinct by construction. `fields` are rendered as
// `key=value key2=value2` into the record's message.
void event(const std::string& name, Level severity,
           std::initializer_list<std::pair<std::string, std::string>> fields = {});

// ttlSec == 0: permanent override (LevelResolver::setCodeTagLevel).
// ttlSec > 0: temporary override that lapses `ttlSec` seconds from now
// (LevelResolver::setTtlTagOverride), using an uptime-based clock - see
// LevelResolver.h, which doesn't care whether "now" is uptime or epoch.
void setLevel(const std::string& tag, Level level, uint32_t ttlSec = 0);

// Forwards to the module's Redactor instance (owned by LogRouter).
void registerSecret(const std::string& secret);

// Applies a remote log_config JSON (plan §3.4/Fase 4, PR15's device
// side - see RemoteLogConfig.h for the exact reduced shape this
// accepts). Wired to MessageBus::subscribeLogConfig()'s callback by the
// project, not by this SDK - see CommonSetup.cpp.
//
// A permanent entry (no `ttlSec`, or `ttlSec: 0`) replaces the whole
// persisted remote layer (LevelResolver's `remote` tier - clearRemote()
// runs first, so an old permanent tag override that the new config
// doesn't mention actually clears rather than lingering) and is written
// to `/journal/log_config.json`, surviving a reboot. A `ttlSec > 0`
// entry goes into LevelResolver's *separate* TTL tier instead - RAM
// only, deliberately not persisted: expressing "this many seconds
// remain" correctly across a reboot needs an absolute instant, which
// needs a real clock (SNTP isn't built yet - see the plan's own Fase 6).
// If the device reboots mid-TTL, the override is simply gone, back to
// the persisted/code layers underneath - a known, accepted simplification
// rather than the "uptime acumulado + nº de arranques" workaround the
// plan sketches for a case this SDK doesn't need to solve yet.
//
// Always emits a `log_config_applied` event (plan §11.5) on success,
// win or lose against the persisted layer, so the server has positive
// confirmation the device actually applied what it sent - not just that
// the MQTT publish succeeded.
Result applyRemoteConfig(const std::string& json);

class SinkHandle {
public:
    SinkHandle& off();
    SinkHandle& level(Level level);
    SinkHandle& detail(Detail detail);

private:
    friend SinkHandle sink(Sink which);
    explicit SinkHandle(Sink which) : which_(which) {}

    Sink which_;
};

SinkHandle sink(Sink which);

// Enables exactly `which` (at `level`) and disables every other sink.
void only(Sink which, Level level);

// No-op until a durable sink exists (PR9) - reserved now so call sites
// already compile against the final shape.
void flushBeforeSleep();

}  // namespace croniot::log

#endif
