#ifndef SENSOR_H
#define SENSOR_H

#include <cstdint>
#include <map>
#include <string>

#include "ReportPolicy.h"
#include "SensorReportBuffer.h"

class Sensor {
public:
    virtual void run() = 0;

    // Plan §7.2. Optional - a sensor that never calls this keeps
    // today's exact behavior: every reportSample()/sendSensorData()
    // call publishes immediately over the legacy per-reading path,
    // unchanged. `samplePeriodMs` is informational only (embedded as a
    // Batch payload's `dtMs`) - this class does not schedule anything
    // itself; nothing here changes how often a subclass calls
    // reportSample(). One policy applies to every sensorUid this
    // instance reports, matching the plan's own
    // `battery->setReporting(...)` example (called on the Sensor
    // object, not per reading).
    void setReporting(uint32_t samplePeriodMs, const croniot::ReportPolicy& policy);

    // Drives time-based Batch flushes - called periodically (not by
    // this class; see SensorsController) with the current uptime.
    // A sensor with nothing buffered (the common case: Immediate,
    // unchanged) does nothing here.
    void maybeFlush(uint64_t nowMs);

protected:
    // Legacy per-reading path (unchanged): publishes `sensorValue`
    // immediately as a JSON string over the existing sensor_data topic.
    // Every sensor that predates this batching support keeps calling
    // this directly and sees no behavior change whatsoever.
    void sendSensorData(int sensorUid, const std::string& sensorValue);

    // Numeric path (plan §7.2): under ReportPolicy::Immediate() (the
    // default if setReporting() was never called), this is exactly
    // sendSensorData(sensorUid, std::to_string(value)) - same wire
    // message, same topic, zero behavior change for a sensor that
    // hasn't opted into batching. Under ReportPolicy::Batch(...), buffers
    // the sample instead and returns without publishing anything;
    // maybeFlush() (or an immediate flush the buffer itself signals -
    // see SensorReportBuffer::add()) is what actually sends a batch, via
    // the Data-stream Journal/Uplink pipeline (CBOR, not JSON, and a
    // completely different topic - see Sensors/SensorBatchEncoder.h).
    void reportSample(int sensorUid, double value);

private:
    void flushSensor(int sensorUid, croniot::SensorReportBuffer& buffer);

    uint32_t samplePeriodMs_ = 0;
    croniot::ReportPolicy policy_ = croniot::ReportPolicy::Immediate();
    std::map<int, croniot::SensorReportBuffer> buffers_;
};

#endif
