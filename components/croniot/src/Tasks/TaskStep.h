#ifndef CRONIOT_TASKS_TASKSTEP_H
#define CRONIOT_TASKS_TASKSTEP_H

#include <string>

#include "log/Log.h"

namespace croniot::task {

// Plan §7.6: a structured, auditable record of one task's step -
// "¿se cerró la válvula?" - routed through croniot::log::event() so it
// gets the exact same durability/uplink treatment as any other event
// (Journal's Events stream, Uplink's ack/retry, dedup by seq), tagged
// "task.<uid>" per the plan so a project's own log_config (see
// RemoteLogConfig.h) can single out one task's steps for verbosity
// without touching the rest. Header-only and not host-tested: it's a
// one-line call into croniot::log::event(), which is itself platform-
// only (Log.cpp isn't part of test_host's compiled sources - the same
// reason Uplink.cpp/Health.cpp aren't host-tested either).
inline void step(long taskUid, const std::string& stepName, bool ok) {
    croniot::log::event("task." + std::to_string(taskUid), croniot::log::Level::Info,
                         {{"step", stepName}, {"ok", ok ? "true" : "false"}});
}

}  // namespace croniot::task

#endif
