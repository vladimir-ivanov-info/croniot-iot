#include "Log.h"

#include <array>

#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "LevelResolver.h"
#include "LogRecord.h"
#include "LogRouter.h"
#include "LogTask.h"

namespace croniot::log {

#if CONFIG_CRONIOT_LOG_ENABLE

namespace {

struct SinkState {
    bool enabled;
    Level level;
    Detail detail;
};

LevelResolver g_resolver;
LogConfig g_config;

// Every tag croniot::log has ever been explicitly told about (via
// LogConfig::tags at init() or a later setLevel() call) - kept here only
// so recomputeEspLogLevels() knows which tags to call
// esp_log_level_set(tag, ...) for. LevelResolver itself has no public way
// to enumerate its internal maps (by design - see LevelResolver.h, it's a
// pure resolver, not a registry), so this mirrors just the *keys* rather
// than touching that pure-logic file for what is purely a platform-layer
// bookkeeping need. Known gap: a tag added via a TTL override
// (setLevel(tag, level, ttlSec)) that later lapses keeps its
// esp_log_level_set() ceiling until the *next* recompute (any setLevel/
// sink/only/init call) - there's no timer driving a refresh purely from
// TTL expiry in PR7.
std::map<std::string, Level> g_tagOverrides;

std::array<SinkState, 3> g_sinks = {{
    {true, Level::Info, Detail::Normal},    // Sink::Console
    {false, Level::Info, Detail::Normal},   // Sink::Flash - no-op until PR9
    {false, Level::Info, Detail::Normal},   // Sink::Sd - no-op until PR21
}};

uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }

esp_log_level_t toEspLogLevel(Level level) {
    switch (level) {
        case Level::Error: return ESP_LOG_ERROR;
        case Level::Warn: return ESP_LOG_WARN;
        case Level::Info: return ESP_LOG_INFO;
        case Level::Debug: return ESP_LOG_DEBUG;
        case Level::Trace: return ESP_LOG_VERBOSE;
    }
    return ESP_LOG_INFO;
}

void applyConsoleSinkToTask() {
    const SinkState& console = g_sinks[static_cast<size_t>(Sink::Console)];
    LogTask::setConsoleEnabled(console.enabled);
    LogTask::setConsoleLevel(console.level);
}

// Global esp_log ceiling = the most verbose level anything currently
// needs: either the configured capture threshold (the ring/RTC black box
// keeps working regardless of sinks) or any *enabled* sink's own level
// (a sink asking for more detail than `capture` must be able to get it).
// This is what makes turning sinks off/down actually reduce work: IDF's
// own ESP_LOGx macros skip formatting and never call the hook at all for
// a call below this threshold (see esp_log_level_set() docs) - lowering
// it is a real, measured cost saving, not just fewer console bytes.
void recomputeEspLogLevels() {
    Level ceiling = g_config.capture;
    for (const auto& sinkState : g_sinks) {
        if (sinkState.enabled && static_cast<uint8_t>(sinkState.level) > static_cast<uint8_t>(ceiling)) {
            ceiling = sinkState.level;
        }
    }
    esp_log_level_set("*", toEspLogLevel(ceiling));

    uint64_t now = nowMs();
    for (const auto& [tag, level] : g_tagOverrides) {
        (void)level;  // the stored value may be stale under a TTL override - ask the resolver, not the map, for the level actually in effect right now.
        esp_log_level_set(tag.c_str(), toEspLogLevel(g_resolver.effectiveLevel(tag, now)));
    }
}

}  // namespace

void init(const LogConfig& config) {
    g_config = config;
    g_resolver.setCodeDefault(config.capture);
    for (const auto& [tag, level] : config.tags) {
        g_resolver.setCodeTagLevel(tag, level);
        g_tagOverrides[tag] = level;
    }

    LogTask::setConsoleFormat(config.consoleFormat);
    applyConsoleSinkToTask();
    recomputeEspLogLevels();

    LogRouter::instance().install();
    LogTask::start();
}

void event(const std::string& name, Level severity,
           std::initializer_list<std::pair<std::string, std::string>> fields) {
    std::string message;
    bool first = true;
    for (const auto& field : fields) {
        if (!first) message += ' ';
        first = false;
        message += field.first;
        message += '=';
        message += field.second;
    }

    LogRecord record{};
    record.level = severity;
    record.setTag(name.c_str());
    record.setMessage(message.c_str());
    LogRouter::instance().pushEvent(record);
}

void setLevel(const std::string& tag, Level level, uint32_t ttlSec) {
    if (ttlSec == 0) {
        g_resolver.setCodeTagLevel(tag, level);
    } else {
        g_resolver.setTtlTagOverride(tag, level, nowMs() + static_cast<uint64_t>(ttlSec) * 1000);
    }
    g_tagOverrides[tag] = level;
    recomputeEspLogLevels();
}

void registerSecret(const std::string& secret) { LogRouter::instance().registerSecret(secret); }

SinkHandle& SinkHandle::off() {
    g_sinks[static_cast<size_t>(which_)].enabled = false;
    if (which_ == Sink::Console) applyConsoleSinkToTask();
    recomputeEspLogLevels();
    return *this;
}

SinkHandle& SinkHandle::level(Level level) {
    SinkState& state = g_sinks[static_cast<size_t>(which_)];
    state.enabled = true;
    state.level = level;
    if (which_ == Sink::Console) applyConsoleSinkToTask();
    recomputeEspLogLevels();
    return *this;
}

SinkHandle& SinkHandle::detail(Detail detail) {
    SinkState& state = g_sinks[static_cast<size_t>(which_)];
    state.enabled = true;
    // Stored, not yet acted on: ConsoleSink (PR7's only sink) always
    // renders Normal-equivalent content - same "config surface reserved,
    // behavior deferred" pattern as Profile (see Log.h).
    state.detail = detail;
    if (which_ == Sink::Console) applyConsoleSinkToTask();
    recomputeEspLogLevels();
    return *this;
}

SinkHandle sink(Sink which) { return SinkHandle(which); }

void only(Sink which, Level level) {
    for (size_t i = 0; i < g_sinks.size(); ++i) {
        g_sinks[i].enabled = (static_cast<Sink>(i) == which);
    }
    g_sinks[static_cast<size_t>(which)].level = level;
    applyConsoleSinkToTask();
    recomputeEspLogLevels();
}

void flushBeforeSleep() {
    // No durable sink exists yet (PR9) - nothing to flush. Reserved so
    // call sites that need this before a future deep-sleep path already
    // compile against the final shape.
}

#else  // CONFIG_CRONIOT_LOG_ENABLE=n: kill switch, everything is a no-op.
       // No hook installed, no task created, no state kept.

void init(const LogConfig&) {}
void event(const std::string&, Level, std::initializer_list<std::pair<std::string, std::string>>) {}
void setLevel(const std::string&, Level, uint32_t) {}
void registerSecret(const std::string&) {}
SinkHandle& SinkHandle::off() { return *this; }
SinkHandle& SinkHandle::level(Level) { return *this; }
SinkHandle& SinkHandle::detail(Detail) { return *this; }
SinkHandle sink(Sink which) { return SinkHandle(which); }
void only(Sink, Level) {}
void flushBeforeSleep() {}

#endif

}  // namespace croniot::log
