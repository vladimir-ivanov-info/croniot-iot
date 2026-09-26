#ifndef CRONIOT_LOG_INCIDENTRECOVERY_H
#define CRONIOT_LOG_INCIDENTRECOVERY_H

namespace croniot::log {

// Platform glue for PR10 ("recuperación al arrancar"): decides whether
// this boot follows a crash worth reporting and, if so, raises a single
// `incident_previous_boot` event summarizing it (reset cause, how much
// leftover .noinit/RTC data there was, and a coredump task/PC/backtrace
// if espcoredump has one). See IncidentDetector.h for the pure decision
// logic this drives.
//
// Must run after LogRouter::instance().install() (so NoinitRing/
// RtcCriticalStore have already validated themselves and their
// size()/count() reflect real leftover data) and after Journal::init()
// (so the event can be durably journaled), but before LogTask::start()
// (so nothing has drained the ring yet when this inspects it) - see the
// call site in Log.cpp::init().
class IncidentRecovery {
public:
    static void run();
};

}  // namespace croniot::log

#endif
